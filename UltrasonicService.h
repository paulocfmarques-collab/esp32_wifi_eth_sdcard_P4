#pragma once
#include <Arduino.h>
#include <math.h>

class UltrasonicService {
public:
    struct Sample {
        bool available = false, angleKnown = false;
        float distanceCm = NAN;
        int angle = 0;
        uint32_t ageMs = 0, samples = 0, errors = 0;
        uint32_t sequence = 0, servoMoveAt = 0;
        uint32_t measuredAt = 0, rejectedJumps = 0;
        float batchCm = NAN;
        bool positionKnown = false;
        const char* error = "";
    };
    void begin();
    void handle(bool paused);
    bool busy() const { return _waiting; }
    bool enabled() const { return _ready && _enabled && !_paused; }
    Sample snapshot() const;
    String status() const;
    String command(String argument);
private:
    bool _ready = false, _enabled = true, _paused = false, _waiting = false;
    bool _valid = false, _angleKnown = false;
    float _distance = NAN, _batchDistance = NAN;
    float _confirmedDistance = NAN, _pendingDistance = NAN;
    uint32_t _confirmedAt = 0, _rejectedJumps = 0;
    float _echoDistances[3]{};
    uint8_t _echoCount = 0, _validEchoCount = 0;
    uint32_t _batchMoveAt = 0;
    int _batchAngle = 0;
    bool _batchAngleKnown = false;
    int _sampleAngle = 0;
    int _pendingAngle = 0;
    bool _pendingAngleKnown = false;
    uint32_t _pendingMoveAt = 0, _triggerUs = 0, _lastTrigger = 0, _sampleAt = 0;
    uint32_t _samples = 0, _errors = 0, _lastLog = 0;
    uint32_t _sequence = 0, _sampleMoveAt = 0;
    const char* _error = "aguardando leitura";
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    volatile bool _armed = false, _riseSeen = false, _complete = false;
    volatile uint32_t _riseUs = 0, _pulseUs = 0;
    static void ARDUINO_ISR_ATTR echo(void* context);
    void cancel();
    void finish(uint32_t pulseUs, const char* error);
};
extern UltrasonicService ultrasonic;
