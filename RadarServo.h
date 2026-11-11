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
    bool trackingEnabled() const { return _trackingEnabled; }
    bool targetLocked() const { return enabled() && _sweep && _locked; }
    int trackedAngle() const { return _trackCenter; }
    float trackedDistance() const { return targetLocked() ? _trackDistance : NAN; }
    const char* mode() const {
        return !_ready ? "falha" : !_enabled ? "desligado" : _paused ? "pausado" :
            !_sweep ? "fixo" : _locked ? "acompanhando" : _confirmations ? "confirmando" : "varrendo";
    }
private:
    bool _ready = false, _enabled = false, _sweep = true, _paused = false;
    int _angle = 90, _target = 90, _direction = 1;
    uint32_t _lastStep = 0;
    uint32_t _motionFrame = 0;
    uint32_t _lastMoveAt = 0;
    bool _trackingEnabled = false, _locked = false;
    uint8_t _confirmations = 0, _probePhase = 0, _lostCycles = 0;
    int _trackCenter = 90, _probeAngles[3] = {90, 84, 96};
    float _trackDistance = NAN, _candidateDistance = NAN, _probeDistances[3] = {};
    bool _probeValid[3] = {};
    uint32_t _lastSonarSequence = 0, _lastSeenAt = 0;
    uint32_t _candidateAt = 0;
    bool _searchEndSeen = false, _returningToEcho = false;
    int _bestSearchAngle = 90;
    float _bestSearchDistance = NAN;
    void resetTracking();
    void sonarSweep();
    void moveTo(int angle);
    void nextSearchStep();
    void startProbeCycle();
    bool writeAngle();
};
extern RadarServo radarServo;
