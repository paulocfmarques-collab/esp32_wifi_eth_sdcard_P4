#pragma once
#include <Arduino.h>
#include <atomic>
#include <esp_log.h>
#include <freertos/queue.h>
class StorageManager;
class SystemLog {
public:
    void begin();
    void record(const char* category, const char* format, ...);
    void process(StorageManager& storage, bool busy);
    String status() const;
    uint32_t dropped() const { return _dropped; }
    uint32_t written() const { return _written; }
    static String sanitize(const String& text);
private:
    struct Entry { uint64_t uptime; char category[12]; char message[200]; };
    QueueHandle_t _queue = nullptr;
    std::atomic<uint32_t> _dropped{0}, _written{0};
    std::atomic<bool> _writing{false};
    std::atomic_flag _hookBusy = ATOMIC_FLAG_INIT;
    uint32_t _nextFlush = 0;
    vprintf_like_t _previous = nullptr;
    static int capture(const char* format, va_list args);
};
extern SystemLog systemLog;
