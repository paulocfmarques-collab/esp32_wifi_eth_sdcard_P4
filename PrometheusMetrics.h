#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "StorageManager.h"
#include <esp_timer.h>
#include <math.h>
#include <atomic>

class PrometheusMetrics {
public:
    explicit PrometheusMetrics(StorageManager& storage) : _storage(storage), _server(80) {}
    void begin();
    void handle(bool otaBusy);
    String page();
    bool save(const String& host, const String& port, const String& interval, String& error, int& status);
    String configurationJson() const;
    String scrapeConfiguration() const;
    void serveOnWeb(WebServer& server);
    void recordRequest(uint64_t startedUs, bool error);
    bool resetCounters();
    void wifiDisconnected() { _disconnects.fetch_add(1); }
    void udpListenerOpened(bool up) { _udpListenerUp = up; }
    void udpPacketReceived() { ++_udpReceived; }
    void udpReplyResult(bool sent) { if (sent) ++_udpReplies; else ++_udpSendErrors; }
    uint64_t udpSendErrors() const { return _udpSendErrors; }
    uint64_t udpReceived() const { return _udpReceived; }
    uint64_t udpReplies() const { return _udpReplies; }
    uint16_t configuredPort() const { return _port; }
private:
    StorageManager& _storage;
    WebServer _server;
    String _host;
    uint16_t _port = 80, _activePort = 80, _interval = 15;
    bool _pending = false;
    uint32_t _savedAt = 0;
    uint64_t _boots = 0, _reboots = 0, _watchdogs = 0;
    uint64_t _requests = 0, _errors = 0;
    bool _udpListenerUp = false;
    uint64_t _udpReceived = 0, _udpReplies = 0, _udpSendErrors = 0;
    std::atomic<uint32_t> _disconnects{0};
    double _responseMs = 0;
    bool _countersSaved = false;
    uint64_t _cpuAt = 0, _idle[2] = {};
    float _cpu = NAN;
    void apply();
    void sampleCpu();
    void metrics(WebServer& server);
};
