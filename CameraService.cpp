#include "CameraService.h"
#include "PortalUi.h"
#include "SystemLog.h"
#include <ArduinoJson.h>
#include <esp32-hal-i2c.h>
#include <esp_heap_caps.h>
#include <math.h>

CameraService camera;

namespace {
void oledFrame(const uint8_t* rgb, unsigned width, unsigned height, uint8_t* out) {
    // Centre-crop to 2:1, then ordered dithering into SSD1306's vertical byte layout.
    static const uint8_t bayer[4][4] = {{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    memset(out, 0, 1024);
    unsigned cropWidth = min(width, height * 2), cropHeight = cropWidth / 2;
    unsigned left = (width - cropWidth) / 2, top = (height - cropHeight) / 2;
    for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 128; ++x) {
        unsigned sx = left + (2*x+1) * cropWidth / 256;
        unsigned sy = top + (2*y+1) * cropHeight / 128;
        size_t offset = (size_t(sy) * width + sx) * 2;
        uint16_t pixel = uint16_t(rgb[offset]) | (uint16_t(rgb[offset+1]) << 8);
        unsigned r = ((pixel >> 11) & 31) * 255 / 31;
        unsigned g = ((pixel >> 5) & 63) * 255 / 63;
        unsigned b = (pixel & 31) * 255 / 31;
        unsigned luminance = (77*r + 150*g + 29*b) >> 8;
        if (luminance > bayer[y%4][x%4] * 16 + 8) out[x + (y/8)*128] |= 1 << (y%8);
    }
}
}

void CameraService::fail(const char* message) {
    _error = message; ++_errors; _ready = false;
    _capture.end(); _video.end();
    if (_encoder) { jpeg_del_encoder_engine(_encoder); _encoder = nullptr; }
    for (auto& buffer : _buffers) { free(buffer); buffer = nullptr; }
    if (_mutex) { vSemaphoreDelete(_mutex); _mutex = nullptr; }
    Serial.printf("[CAMERA] %s\n", message);
    systemLog.record("CAMERA", "falha: %s", message);
}

void CameraService::begin() {
    // Reuse Wire's IDF bus: allocating a second bus on GPIO7/8 would detach OLED/audio.
    auto bus = static_cast<i2c_master_bus_handle_t>(i2cBusHandle(0));
    if (!bus || !psramFound()) { fail("I2C Wire ou PSRAM indisponivel"); return; }
    ESPVideoCamConfigClass sensor;
    sensor.begin(bus, 100000);
    ESPVideoCSIConfigClass csi; csi.begin(sensor);
    if (!_video.begin(csi)) { fail("OV5647 nao detectada: confira cabo CSI e alimentacao"); return; }
    if (!_capture.begin(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, 2) || !_capture.setFormat(ESP_VIDEO_FORMAT_RGB565)) {
        fail("Falha ao abrir captura RGB565"); return;
    }
    _width = _capture.getWidth(); _height = _capture.getHeight();
    if (!_width || !_height || _width > 1280 || _height > 1280 || _width % 16 || _height % 16) {
        fail("Resolucao da camera fora do limite 1280x1280 ou alinhamento JPEG"); return;
    }
    jpeg_encode_engine_cfg_t engine = {}; engine.timeout_ms = 200;
    if (jpeg_new_encoder_engine(&engine, &_encoder) != ESP_OK) { fail("Falha ao criar encoder JPEG"); return; }
    jpeg_encode_memory_alloc_cfg_t memory = {}; memory.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER;
    size_t requested = size_t(_width) * _height * 3 + 1024;
    _buffers[0] = static_cast<uint8_t*>(jpeg_alloc_encoder_mem(requested, &memory, &_capacity));
    size_t secondCapacity = 0;
    _buffers[1] = static_cast<uint8_t*>(jpeg_alloc_encoder_mem(requested, &memory, &secondCapacity));
    _mutex = xSemaphoreCreateMutex();
    if (!_buffers[0] || !_buffers[1] || !_mutex) { fail("Memoria insuficiente para JPEG"); return; }
    _capacity = min(_capacity, secondCapacity);
    if (!_capture.startCapture()) { fail("Falha ao iniciar captura CSI"); return; }
    if (xTaskCreatePinnedToCore(run, "camera_jpeg", 8192, this, 1, &_task, 0) != pdPASS) {
        _task = nullptr; fail("Falha ao criar tarefa da camera"); return;
    }
    _error = "nenhum"; _ready = true;
    systemLog.record("CAMERA", "OV5647 iniciada %ux%u", _width, _height);
    Serial.printf("[CAMERA] OV5647 CSI: %ux%u; JPEG ate 2 imagens/s.\n", _width, _height);
}

void CameraService::run(void* context) {
    auto& self = *static_cast<CameraService*>(context);
    uint32_t nextEncode = 0;
    for (;;) {
        if (self._paused) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        // DQBUF can wait for the sensor: only this worker waits, never the web/application task.
        auto frame = self._capture.captureBuffer();
        if (!frame.valid()) { self._captureError = 1; ++self._errors; vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        if (int32_t(millis() - nextEncode) < 0) { vTaskDelay(1); continue; }
        nextEncode = millis() + 500;
        if (self._paused) continue;
        if (frame.size() < size_t(self._width) * self._height * 2) { self._captureError = 2; ++self._errors; continue; }
        jpeg_encode_cfg_t config = {};
        config.width = self._width; config.height = self._height;
        config.src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
        config.sub_sample = JPEG_DOWN_SAMPLING_YUV422; config.image_quality = 70;
        unsigned back = 1 - self._front;
        uint32_t size = 0;
        if (jpeg_encoder_process(self._encoder, &config, frame.data(), frame.size(),
                                 self._buffers[back], self._capacity, &size) != ESP_OK || !size || size > self._capacity) {
            self._captureError = 3; ++self._errors; continue;
        }
        uint8_t mono[1024];
        oledFrame(frame.data(), self._width, self._height, mono);
        if (xSemaphoreTake(self._mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            self._front = back; self._length = size; self._lastFrameMs = millis(); ++self._frames;
            memcpy(self._monochrome, mono, sizeof(mono));
            self._captureError = 0;
            xSemaphoreGive(self._mutex);
        }
    }
}

double CameraService::ageSeconds() const {
    return _frames.load() ? uint32_t(millis() - _lastFrameMs.load()) / 1000.0 : NAN;
}

bool CameraService::copyOled(uint8_t* target, size_t size) {
    if (!target || size != sizeof(_monochrome) || !available() ||
        xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
    memcpy(target, _monochrome, size); xSemaphoreGive(_mutex); return true;
}

String CameraService::status() {
    JsonDocument doc;
    doc["model"] = "OV5647"; doc["interface"] = "MIPI-CSI";
    doc["ready"] = ready(); doc["paused"] = _paused.load(); doc["error"] = _error;
    doc["available"] = available();
    doc["stream_port"] = _streamPort; doc["stream_path"] = "/stream";
    doc["stream_clients"] = streamClients(); doc["stream_frames"] = streamFrames(); doc["stream_errors"] = streamErrors();
    doc["snapshot_clients"] = _snapshotClients.load(); doc["rejected_clients"] = _rejectedClients.load();
    doc["heap_free"] = ESP.getFreeHeap(); doc["psram_free"] = ESP.getFreePsram();
    switch (_captureError.load()) {
        case 1: doc["capture_error"] = "falha ao receber frame"; break;
        case 2: doc["capture_error"] = "frame RGB565 incompleto"; break;
        case 3: doc["capture_error"] = "falha ou timeout do encoder JPEG"; break;
        default: doc["capture_error"] = frames() ? "nenhum" : "aguardando primeiro JPEG";
    }
    doc["width"] = _width; doc["height"] = _height; doc["frames"] = frames(); doc["errors"] = errors();
    double age = ageSeconds(); if (isfinite(age)) doc["age_seconds"] = age; else doc["age_seconds"] = nullptr;
    String json; serializeJson(doc, json); return json;
}

uint8_t* CameraService::copyFrame(size_t& length) {
    length = 0;
    if (!available() || !_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return nullptr;
    length = _length;
    auto copy = static_cast<uint8_t*>(heap_caps_malloc(length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (copy) memcpy(copy, _buffers[_front], length);
    xSemaphoreGive(_mutex);
    return copy;
}

void CameraService::snapshot(WebServer& server) {
    if (!_streamPort || !available()) { server.send(503, "text/plain", "Camera sem imagem recente ou servidor indisponivel"); return; }
    // The application's WebServer must never send an entire image to a slow viewer.
    String destination = "http://" + server.client().localIP().toString() + ":" + String(_streamPort) + "/snapshot";
    server.sendHeader("Location", destination);
    server.send(302, "text/plain", "JPEG no servidor da camera");
}

void CameraService::startStreaming(uint16_t metricsPort) {
    if (_streamServer || !_ready) return;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 6144; config.max_open_sockets = 4;
    config.lru_purge_enable = true; config.recv_wait_timeout = 2; config.send_wait_timeout = 2;
    // Preserve an existing Prometheus port; also fall back if another service owns TCP81.
    for (uint16_t port = 81; port <= 89; ++port) {
        if (port == metricsPort) continue;
        config.server_port = port;
        if (httpd_start(&_streamServer, &config) == ESP_OK) {
            httpd_uri_t route = {}; route.uri = "/stream"; route.method = HTTP_GET;
            route.handler = streamRequest; route.user_ctx = this;
            esp_err_t registered = httpd_register_uri_handler(_streamServer, &route);
            route.uri = "/snapshot"; route.handler = snapshotRequest;
            if (registered == ESP_OK && httpd_register_uri_handler(_streamServer, &route) == ESP_OK) {
                _streamPort = port;
                Serial.printf("[CAMERA] MJPEG: http://IP:%u/stream (ate 2 clientes).\n", port);
                return;
            }
            httpd_stop(_streamServer); _streamServer = nullptr;
        }
    }
    ++_streamErrors;
}

esp_err_t CameraService::streamRequest(httpd_req_t* request) {
    auto& self = *static_cast<CameraService*>(request->user_ctx);
    if (!self.available()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(request, "Camera sem imagem recente ou pausada");
    }
    if (self._streamClients.fetch_add(1) >= 2) {
        --self._streamClients;
        ++self._rejectedClients;
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(request, "Limite de dois clientes de video atingido");
    }
    httpd_req_t* async = nullptr;
    if (httpd_req_async_handler_begin(request, &async) != ESP_OK) {
        --self._streamClients; ++self._streamErrors; return ESP_FAIL;
    }
    if (xTaskCreatePinnedToCore(streamWorker, "mjpeg_client", 6144, async, 1, nullptr, 0) != pdPASS) {
        httpd_resp_set_status(async, "503 Service Unavailable");
        httpd_resp_sendstr(async, "Memoria insuficiente para video");
        finishRequest(async); --self._streamClients; ++self._streamErrors;
    }
    return ESP_OK;
}

void CameraService::streamWorker(void* context) {
    auto request = static_cast<httpd_req_t*>(context);
    auto& self = *static_cast<CameraService*>(request->user_ctx);
    httpd_resp_set_type(request, "multipart/x-mixed-replace;boundary=p4frame");
    systemLog.record("VIDEO", "stream iniciado clientes=%lu", (unsigned long)self.streamClients());
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Connection", "close");
    uint32_t previous = 0;
    esp_err_t result = ESP_OK;
    while (self.available()) {
        uint32_t sequence = self.frames();
        if (previous == sequence) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        size_t length;
        auto jpeg = self.copyFrame(length);
        if (!jpeg) { ++self._streamErrors; break; }
        char header[120];
        int count = snprintf(header, sizeof(header), "--p4frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", unsigned(length));
        result = httpd_resp_send_chunk(request, header, count);
        if (result == ESP_OK) result = httpd_resp_send_chunk(request, reinterpret_cast<char*>(jpeg), length);
        free(jpeg);
        if (result == ESP_OK) result = httpd_resp_send_chunk(request, "\r\n", 2);
        if (result != ESP_OK) { ++self._streamErrors; break; }
        ++self._streamFrames; previous = sequence;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (result == ESP_OK) {
        httpd_resp_send_chunk(request, "--p4frame--\r\n", HTTPD_RESP_USE_STRLEN);
        httpd_resp_send_chunk(request, nullptr, 0);
    }
    finishRequest(request);
    --self._streamClients;
    systemLog.record("VIDEO", "stream encerrado resultado=%d clientes=%lu", int(result), (unsigned long)self.streamClients());
    vTaskDelete(nullptr);
}

void CameraService::finishRequest(httpd_req_t* request) {
    httpd_handle_t server = request->handle;
    int socket = httpd_req_to_sockfd(request);
    // Release async ownership before asking HTTPD to reclaim the session/socket.
    httpd_req_async_handler_complete(request);
    if (socket >= 0) httpd_sess_trigger_close(server, socket);
}

esp_err_t CameraService::snapshotRequest(httpd_req_t* request) {
    auto& self = *static_cast<CameraService*>(request->user_ctx);
    httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "*"); // Read-only JPEG, no credentials or command endpoint.
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    if (!self.available()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(request, "Camera sem imagem recente ou pausada");
    }
    if (self._snapshotClients.fetch_add(1) >= 1) {
        --self._snapshotClients; ++self._rejectedClients;
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(request, "Uma foto ja esta sendo enviada; tente novamente");
    }
    httpd_req_t* async = nullptr;
    if (httpd_req_async_handler_begin(request, &async) != ESP_OK) {
        --self._snapshotClients; ++self._streamErrors; return ESP_FAIL;
    }
    if (xTaskCreatePinnedToCore(snapshotWorker, "jpeg_client", 4096, async, 1, nullptr, 0) != pdPASS) {
        httpd_resp_set_status(async, "503 Service Unavailable");
        httpd_resp_sendstr(async, "Memoria insuficiente para foto");
        finishRequest(async); --self._snapshotClients; ++self._streamErrors;
    }
    return ESP_OK;
}

void CameraService::snapshotWorker(void* context) {
    auto request = static_cast<httpd_req_t*>(context);
    auto& self = *static_cast<CameraService*>(request->user_ctx);
    httpd_resp_set_hdr(request, "Connection", "close");
    size_t length;
    auto jpeg = self.copyFrame(length);
    if (!jpeg) {
        ++self._streamErrors;
        httpd_resp_set_status(request, "503 Service Unavailable");
        httpd_resp_sendstr(request, "Camera ocupada ou PSRAM insuficiente");
    } else {
        httpd_resp_set_type(request, "image/jpeg");
        if (httpd_resp_send(request, reinterpret_cast<char*>(jpeg), length) != ESP_OK) ++self._streamErrors;
        free(jpeg);
    }
    finishRequest(request);
    --self._snapshotClients;
    vTaskDelete(nullptr);
}

void CameraService::routes(WebServer& server, bool (*busy)()) {
    server.on("/camera", HTTP_GET, [this, &server]() { server.sendHeader("Cache-Control", "no-store"); server.send(200, "text/html; charset=utf-8", page()); });
    server.on("/api/camera/status", HTTP_GET, [this, &server]() { server.sendHeader("Cache-Control", "no-store"); server.send(200, "application/json", status()); });
    server.on("/camera.jpg", HTTP_GET, [this, &server, busy]() {
        server.sendHeader("Cache-Control", "no-store");
        if (busy && busy()) { server.send(503, "text/plain", "Aguarde OTA ou reinicio"); return; }
        snapshot(server);
    });
}

String CameraService::page() {
    return portalStart("Camera", false, false, false, false, false, true) +
        F("<header class='page-header'><h1>Câmera OV5647</h1></header><section class='card'><p>Vídeo MJPEG até 2 imagens por segundo, com até dois espectadores. Captura local, sem enviar imagens ao broker MQTT.</p>") +
        FPSTR(CAMERA_VIEW) + F("</section>") + portalEnd();
}
