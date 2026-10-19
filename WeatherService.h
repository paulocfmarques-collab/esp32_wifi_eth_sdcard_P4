#pragma once
#include <Arduino.h>
#include <atomic>
#include <math.h>
#include <time.h>
#include "freertos/task.h"

class WeatherService {
public:
    struct State {
        bool hasData = false, available = false, attempted = false, lastRequestOk = false;
        float temperature = NAN, humidity = NAN;
        time_t observedAt = 0;
        uint64_t receivedUs = 0;
        int httpStatus = 0;
        int error = 0;
        double ageSeconds = NAN;
    };
    void begin();
    void suspend(bool value) { _suspended.store(value); }
    State snapshot();
    String status();
private:
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    TaskHandle_t _task = nullptr;
    std::atomic<bool> _suspended{false};
    State _state;
    static void run(void* context);
    bool fetch();
};
extern WeatherService weather;
