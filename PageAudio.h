#pragma once
#include <Arduino.h>
#include <atomic>
#include "driver/i2s_std.h"
#include "freertos/task.h"

class PageAudio {
public:
    void begin();
    bool play(bool diagnostic = false);
    void suspend(bool value) { _suspended.store(value); }
    String status();
private:
    i2s_chan_handle_t _tx = nullptr;
    TaskHandle_t _task = nullptr;
    std::atomic<bool> _ready{false}, _suspended{false};
    std::atomic<bool> _diagnostic{false};
    std::atomic<int> _error{ESP_OK};
    std::atomic<uint32_t> _requested{0}, _completed{0}, _failures{0}, _lastBytes{0};
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t& value);
    bool writeSamples(const int16_t* samples, size_t bytes);
    void fail(esp_err_t error);
    static void run(void* context);
    void tone(bool diagnostic);
};
extern PageAudio pageAudio;
