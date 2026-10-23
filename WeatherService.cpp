#include "WeatherService.h"
#include "DeviceConfig.h"
#include "SystemLog.h"
#include <WiFi.h>
#include <ETH.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_timer.h>

WeatherService weather;
namespace {
enum Error { None, TaskCreation, HttpFailure, ResponseTooLarge, JsonFailure, InvalidData };
bool connected() {
    return WiFi.status() == WL_CONNECTED || (ETH.linkUp() && ETH.localIP() != IPAddress(0,0,0,0));
}
const char* errorName(int code) {
    switch (code) {
        case None: return "nenhum";
        case TaskCreation: return "falha ao criar tarefa";
        case HttpFailure: return "falha HTTPS/HTTP (confira rede, DNS, NTP e certificado)";
        case ResponseTooLarge: return "resposta excede 2048 bytes";
        case JsonFailure: return "JSON incompleto ou invalido";
        default: return "temperatura, unidade ou horario dos dados invalidos";
    }
}
// HTTPClient decodes transfer/chunk framing; this stream caps decoded response memory.
class WeatherBody : public Stream {
public:
    char data[2049] = {};
    size_t length = 0;
    bool overflow = false;
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* bytes, size_t count) override {
        if (count > sizeof(data) - 1 - length) { overflow = true; return 0; }
        memcpy(data + length, bytes, count); length += count; data[length] = 0;
        return count;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};
}

void WeatherService::begin() {
    if (_task) return;
    if (xTaskCreatePinnedToCore(run, "open_meteo", 12288, this, 1, &_task, 0) != pdPASS) {
        _task = nullptr;
        _state.error = TaskCreation;
        Serial.println("[CLIMA] Falha ao criar tarefa Open-Meteo.");
    }
}

void WeatherService::run(void* context) {
    auto& service = *static_cast<WeatherService*>(context);
    uint64_t nextAttempt = 0;
    for (;;) {
        uint64_t now = esp_timer_get_time();
        if (!service._suspended.load() && connected() && time(nullptr) > 1700000000 && now >= nextAttempt) {
            bool ok = service.fetch();
            // Same cadence as M5NanoC6: ten minutes on success, two on failure.
            nextAttempt = esp_timer_get_time() + (ok ? 600000000ULL : 120000000ULL);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool WeatherService::fetch() {
    NetworkClientSecure client;
    client.useBuiltinCACertBundle();
    client.setHandshakeTimeout(8);
    HTTPClient http;
    http.setConnectTimeout(5000); http.setTimeout(5000); http.useHTTP10(true);
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(WEATHER_LATITUDE, 5) +
        "&longitude=" + String(WEATHER_LONGITUDE, 5) +
        "&current=temperature_2m,relative_humidity_2m&temperature_unit=celsius&timeformat=unixtime&timezone=UTC";
    State next;
    next.attempted = true;
    next.error = HttpFailure;
    if (http.begin(client, url)) {
        next.httpStatus = http.GET();
        if (next.httpStatus == HTTP_CODE_OK) {
            WeatherBody body;
            if (http.getSize() > 2048) next.error = ResponseTooLarge;
            else {
                int transferred = http.writeToStream(&body);
                next.error = body.overflow ? ResponseTooLarge : JsonFailure;
                if (!body.overflow && transferred > 0 && body.length > 0) {
                    JsonDocument filter, data;
                    filter["current"]["temperature_2m"] = true;
                    filter["current"]["relative_humidity_2m"] = true;
                    filter["current"]["time"] = true;
                    filter["current_units"]["temperature_2m"] = true;
                    if (!deserializeJson(data, body.data, body.length, DeserializationOption::Filter(filter))) {
                        next.error = InvalidData;
                        JsonVariant temperature = data["current"]["temperature_2m"];
                        JsonVariant observedAt = data["current"]["time"];
                        const char* unit = data["current_units"]["temperature_2m"] | "";
                        if (temperature.is<float>() && observedAt.is<int64_t>() && strcmp(unit, "°C") == 0) {
                            next.temperature = temperature.as<float>();
                            next.observedAt = time_t(observedAt.as<int64_t>());
                            time_t now = time(nullptr);
                            if (isfinite(next.temperature) && next.temperature >= -90 && next.temperature <= 60 &&
                                next.observedAt >= now - 1800 && next.observedAt <= now + 900) {
                                next.hasData = true; next.lastRequestOk = true; next.error = None;
                                JsonVariant humidity = data["current"]["relative_humidity_2m"];
                                if (humidity.is<float>()) {
                                    float value = humidity.as<float>();
                                    if (isfinite(value) && value >= 0 && value <= 100) next.humidity = value;
                                }
                                next.receivedUs = esp_timer_get_time();
                            }
                        }
                    }
                }
            }
        }
        http.end();
    }
    // A failed refresh preserves the previous valid cache; snapshot independently expires it.
    portENTER_CRITICAL(&_mux);
    _state.attempted = true; _state.lastRequestOk = next.lastRequestOk;
    _state.httpStatus = next.httpStatus; _state.error = next.error;
    if (next.lastRequestOk) {
        _state.hasData = true; _state.temperature = next.temperature; _state.humidity = next.humidity;
        _state.observedAt = next.observedAt; _state.receivedUs = next.receivedUs;
    }
    portEXIT_CRITICAL(&_mux);
    if (next.lastRequestOk) Serial.printf("[CLIMA] Open-Meteo Porto Alegre: %.1f C\n", next.temperature);
    else Serial.printf("[CLIMA] HTTP %d: %s; nova tentativa em 2 min.\n", next.httpStatus, errorName(next.error));
    systemLog.record("CLIMA", "HTTP=%d sucesso=%d temperatura=%.1f erro=%d", next.httpStatus, next.lastRequestOk, next.temperature, next.error);
    return next.lastRequestOk;
}

WeatherService::State WeatherService::snapshot() {
    portENTER_CRITICAL(&_mux);
    State result = _state;
    portEXIT_CRITICAL(&_mux);
    time_t now = time(nullptr);
    if (result.hasData && now > 1700000000) {
        result.ageSeconds = max(0.0, difftime(now, result.observedAt));
        result.available = result.ageSeconds < 1800 && result.observedAt <= now + 900 &&
                           uint64_t(esp_timer_get_time()) - result.receivedUs < 1800000000ULL;
    }
    return result;
}

String WeatherService::status() {
    State state = snapshot();
    return String("Clima: Open-Meteo, Porto Alegre (-30.03283, -51.23019)\nRede: ") +
        (connected() ? "conectada" : "sem conexao") + "\nNTP: " +
        (time(nullptr) > 1700000000 ? "sincronizado" : "aguardando") +
        "\nDado valido: " + (state.available ? "sim" : "nao") +
        "\nTemperatura externa: " + (state.available ? String(state.temperature, 2) : String("indisponivel")) +
        " C\nUmidade: " + (state.available && isfinite(state.humidity) ? String(state.humidity, 1) : String("indisponivel")) +
        " %\nIdade do dado: " + (isfinite(state.ageSeconds) ? String(state.ageSeconds, 0) : String("indisponivel")) +
        " s\nUltimo HTTP: " + String(state.httpStatus) + "\nErro: " + errorName(state.error) + "\n";
}
