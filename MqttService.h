#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include "src/ArduinoMqttClient/MqttClient.h"
#include <atomic>
#include "StorageManager.h"
#include "freertos/queue.h"
#include "freertos/task.h"

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
    struct Connection {
        bool enabled = false;
        char host[254]{}, user[129]{}, password[129]{};
        uint16_t port = 1883;
        uint32_t generation = 0;
    };
    struct Publication {
        char topic[160]{}, payload[2048]{};
        bool retain = false;
        uint32_t generation = 0;
    };
    MqttClient* _client = nullptr; // Worker-only pointer, including receive callbacks.
    TaskHandle_t _worker = nullptr;
    QueueHandle_t _commands = nullptr;
    QueueHandle_t _connections = nullptr, _publications = nullptr;
    std::atomic<uint32_t> _generation{0};
    uint32_t _workerGeneration = 0;
    String _id, _base;
    std::atomic<bool> _connected{false}, _announce{false};
    std::atomic<uint32_t> _sent{0}, _received{0}, _errors{0};
    std::atomic<uint32_t> _disconnects{0};
    std::atomic<int> _errorType{0}, _brokerCode{0}, _socketError{0}, _transportError{0};
    uint32_t _publishAt = 0, _applyAt = 0;
    uint32_t _servoPublishAt = 0;
    uint32_t _discoveryAt = 0, _announceAt = 0;
    bool _pending = false;
    size_t _discoveryIndex = 0;
    bool _discovering = false;
    static void run(void*);
    static void message(int size);
    void start();
    bool publish(const String& topic, const String& payload, bool retain = true);
    bool discovery(size_t index);
    void state(StorageManager& storage);
    void servoState();
    String configuration();
    void save(WebServer& server);
};
extern MqttService mqtt;
