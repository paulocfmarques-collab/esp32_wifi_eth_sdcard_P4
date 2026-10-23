#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <mqtt_client.h>
#include <atomic>
#include "StorageManager.h"
#include "freertos/queue.h"

class MqttService {
public:
    void begin();
    void routes(WebServer& server, bool (*busy)());
    void handle(StorageManager& storage, bool busy, void (*command)(String));
    void buttonPressed();
    void edgeEvent(const char* rule, bool active, uint64_t sequence, uint64_t uptime);
    void refreshState() { _publishAt = millis(); }
    bool connected() const { return _connected.load(); }
    uint32_t sent() const { return _sent.load(); }
    uint32_t received() const { return _received.load(); }
    uint32_t errors() const { return _errors.load(); }
    String status();
    String page();
private:
    struct Config { bool enabled = false; String host, user, password; uint16_t port = 1883; } _config;
    struct Command { char text[32]; };
    esp_mqtt_client_handle_t _client = nullptr;
    QueueHandle_t _commands = nullptr;
    String _id, _base;
    std::atomic<bool> _connected{false}, _announce{false};
    std::atomic<uint32_t> _sent{0}, _received{0}, _errors{0};
    std::atomic<uint32_t> _disconnects{0};
    std::atomic<int> _errorType{0}, _brokerCode{0}, _socketError{0}, _transportError{0};
    uint32_t _publishAt = 0, _applyAt = 0;
    uint32_t _discoveryAt = 0, _announceAt = 0;
    bool _pending = false;
    size_t _discoveryIndex = 0;
    bool _discovering = false;
    static void event(void*, esp_event_base_t, int32_t, void*);
    void start();
    void stop();
    bool publish(const String& topic, const String& payload, bool retain = true);
    bool discovery(size_t index);
    void state(StorageManager& storage);
    String configuration();
    void save(WebServer& server);
};
extern MqttService mqtt;
