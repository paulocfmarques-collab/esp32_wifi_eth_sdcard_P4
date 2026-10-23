#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <ESP_Video.h>
#include <driver/jpeg_encode.h>
#include <atomic>
#include <math.h>
#include <freertos/semphr.h>
#include <esp_http_server.h>

class CameraService {
public:
    void begin();
    void routes(WebServer& server, bool (*busy)());
    void startStreaming(uint16_t metricsPort);
    uint16_t streamPort() const { return _streamPort; }
    uint32_t streamClients() const { return _streamClients; }
    uint32_t streamFrames() const { return _streamFrames; }
    uint32_t streamErrors() const { return _streamErrors; }
    void suspend(bool paused) { _paused = paused; }
    String status();
    String page();
    bool ready() const { return _ready; }
    bool available() const { double age = ageSeconds(); return ready() && !_paused && isfinite(age) && age <= 3; }
    uint32_t frames() const { return _frames; }
    uint32_t errors() const { return _errors; }
    double ageSeconds() const;
    bool copyOled(uint8_t* target, size_t size);
private:
    ESPVideoClass _video;
    ESPVideoCaptureDevClass _capture;
    jpeg_encoder_handle_t _encoder = nullptr;
    SemaphoreHandle_t _mutex = nullptr;
    TaskHandle_t _task = nullptr;
    uint8_t* _buffers[2] = {};
    size_t _capacity = 0, _length = 0;
    uint8_t _monochrome[1024] = {};
    unsigned _front = 0, _width = 0, _height = 0;
    std::atomic<bool> _ready{false}, _paused{false};
    std::atomic<uint32_t> _frames{0}, _errors{0}, _lastFrameMs{0};
    std::atomic<unsigned> _captureError{0};
    const char* _error = "nao inicializada";
    httpd_handle_t _streamServer = nullptr;
    uint16_t _streamPort = 0;
    std::atomic<uint32_t> _streamClients{0}, _streamFrames{0}, _streamErrors{0};
    std::atomic<uint32_t> _snapshotClients{0}, _rejectedClients{0};
    void fail(const char* message);
    static void run(void* context);
    void snapshot(WebServer& server);
    uint8_t* copyFrame(size_t& length);
    static esp_err_t streamRequest(httpd_req_t* request);
    static void streamWorker(void* request);
    static void snapshotWorker(void* request);
    static esp_err_t snapshotRequest(httpd_req_t* request);
    static void finishRequest(httpd_req_t* request);
};
extern CameraService camera;
