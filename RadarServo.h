#pragma once
#include <Arduino.h>
#include <math.h>

class RadarServo {
public:
    void begin();
    void handle(bool paused);
    String command(String argument);
    String status() const;
    int angle() const { return _angle; }
    int target() const { return _target; }
    bool enabled() const { return _ready && _enabled && !_paused; }
    uint32_t lastMoveAt() const { return _lastMoveAt; }
    // Compatibility fields for CSV/telemetry readers; target tracking has been removed.
    bool trackingEnabled() const { return false; }
    bool targetLocked() const { return false; }
    int trackedAngle() const { return 0; }
    float trackedDistance() const { return NAN; }
    const char* mode() const {
        return !_ready ? "falha" : !_enabled ? "desligado" : _paused ? "pausado" :
            !_sweep ? "fixo" : "varrendo";
    }
private:
    bool _ready = false, _enabled = false, _sweep = true, _paused = false;
    int _angle = 90, _target = 90, _direction = 1;
    uint32_t _lastStep = 0;
    uint32_t _lastMoveAt = 0;
    bool writeAngle();
};
extern RadarServo radarServo;
