#pragma once
#include <Arduino.h>
#include <math.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include "StorageManager.h"

class EdgeComputing {
public:
    static constexpr unsigned RULE_COUNT = 4;
    void begin();
    void routes(WebServer& server, bool (*busy)());
    void handle(StorageManager& storage, bool busy, bool networkUp, uint64_t udpErrors);
    String status();
    String page();
    bool enabled() const { return _config.enabled; }
    bool active(unsigned i) const { return _rules[i].active; }
    bool valid(unsigned i) const { return _rules[i].valid; }
    uint64_t alerts(unsigned i) const { return _rules[i].alerts; }
    uint64_t events() const { return _events; }
    uint32_t activeCount() const;
    static const char* ruleName(unsigned i);
private:
    struct Config {
        bool enabled = false, sound = false;
        unsigned cpuLimit = 70, heapLimit = 65536, udpLimit = 5;
        unsigned confirmSeconds = 10, cooldownSeconds = 300;
    } _config;
    struct Rule {
        bool active = false, candidate = false, pending = false, valid = false;
        uint64_t since = 0, notifiedAt = 0, alerts = 0;
    } _rules[RULE_COUNT];
    struct Event { uint64_t sequence = 0, uptime = 0; unsigned rule = 0; bool active = false; };
    Event _history[16];
    unsigned _historyNext = 0;
    uint64_t _events = 0, _nextSample = 0, _started = 0, _lastUdp = 0, _udpSecond = 0;
    uint64_t _udpBuckets[60] = {}, _udpWindow = 0;
    bool _baseline = false, _paused = false;
    float _cpu = NAN;
    uint32_t _heap = 0;
    bool _network = false;
    void resetWindow();
    void evaluate(unsigned index, bool bad, bool valid, uint64_t now, StorageManager& storage);
    void notify(unsigned index, bool active, uint64_t now, StorageManager& storage);
    void save(WebServer& server);
    static void encodeConfig(JsonDocument& doc, const Config& config);
    String configuration();
};
extern EdgeComputing edge;
