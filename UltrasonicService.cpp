#include "UltrasonicService.h"
#include "DeviceConfig.h"
#include "RadarServo.h"
#include "SystemLog.h"
#include <ArduinoJson.h>
#include <driver/gpio.h>

UltrasonicService ultrasonic;

void UltrasonicService::begin() {
    pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    pinMode(ULTRASONIC_ECHO_PIN, INPUT_PULLDOWN);
    attachInterruptArg(ULTRASONIC_ECHO_PIN, echo, this, CHANGE);
    _ready = true;
    systemLog.record("SONAR", "HC-SR04 TRIG=%d ECHO=%d; ECHO exige divisor de tensao", ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN);
}

void ARDUINO_ISR_ATTR UltrasonicService::echo(void* context) {
    auto& self = *static_cast<UltrasonicService*>(context);
    const uint32_t now = micros();
    const bool high = gpio_get_level(static_cast<gpio_num_t>(ULTRASONIC_ECHO_PIN));
    portENTER_CRITICAL_ISR(&self._mux);
    if (self._armed) {
        if (high && !self._riseSeen) { self._riseUs = now; self._riseSeen = true; }
        else if (!high && self._riseSeen) {
            self._pulseUs = now - self._riseUs;
            self._complete = true;
            self._armed = false;
        }
    }
    portEXIT_CRITICAL_ISR(&self._mux);
}

void UltrasonicService::cancel() {
    portENTER_CRITICAL(&_mux);
    _armed = _complete = _riseSeen = false;
    portEXIT_CRITICAL(&_mux);
    _waiting = false;
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
}

void UltrasonicService::finish(uint32_t pulseUs, const char* error) {
    cancel();
    if (radarServo.enabled() != _pendingAngleKnown || (_pendingAngleKnown &&
        (radarServo.lastMoveAt() != _pendingMoveAt || radarServo.angle() != _pendingAngle))) {
        _echoCount = _validEchoCount = 0;
        _confirmedDistance = _pendingDistance = NAN;
        _valid = false;
        return;
    }
    const float distance = pulseUs / 58.0f;
    if (!error && distance >= 2.0f && distance <= 400.0f)
        _echoDistances[_validEchoCount++] = distance;

    // Fit an agreeing pair into a 200 ms sweep dwell without mixing servo positions.
    const uint8_t requiredEchoes = radarServo.sweeping() ? 2 : 3;
    if (++_echoCount < requiredEchoes) return;

    _sampleAt = _lastTrigger;
    _batchDistance = NAN;
    float closestGap = INFINITY;
    for (uint8_t i = 0; i < _validEchoCount; ++i) {
        for (uint8_t j = i + 1; j < _validEchoCount; ++j) {
            const float a = _echoDistances[i], b = _echoDistances[j];
            const float gap = fabsf(a - b);
            if (gap < closestGap && gap <= max(3.0f, min(a, b) * 0.10f)) {
                closestGap = gap;
                _batchDistance = (a + b) / 2.0f;
            }
        }
    }
    _valid = isfinite(_batchDistance);
    _error = _valid ? "nenhum" : "ecos insuficientes ou divergentes";
    if (_valid) {
        const bool recent = isfinite(_confirmedDistance) && _sampleAt - _confirmedAt <= 1500;
        const bool jump = recent && fabsf(_batchDistance - _confirmedDistance) >
            max(8.0f, _confirmedDistance * 0.15f);
        const bool confirmedJump = isfinite(_pendingDistance) &&
            fabsf(_batchDistance - _pendingDistance) <= max(3.0f, _pendingDistance * 0.10f);
        if (jump && !confirmedJump) {
            _pendingDistance = _batchDistance;
            _valid = false;
            ++_rejectedJumps;
            _error = "salto de distancia aguardando confirmacao";
        } else {
            _confirmedDistance = _batchDistance;
            _confirmedAt = _sampleAt;
            _pendingDistance = NAN;
        }
    } else {
        _pendingDistance = NAN;
    }
    _distance = _valid ? _batchDistance : NAN;
    if (_valid) ++_samples;
    else ++_errors;
    _echoCount = _validEchoCount = 0;
    _sampleAngle = _pendingAngle;
    _sampleMoveAt = _pendingMoveAt;
    ++_sequence;
    _angleKnown = _pendingAngleKnown && radarServo.enabled() && radarServo.lastMoveAt() == _pendingMoveAt;
}

void UltrasonicService::handle(bool paused) {
    if (!_ready) return;
    _paused = paused;
    if (_paused || !_enabled) { cancel(); _echoCount = _validEchoCount = 0; _confirmedDistance = _pendingDistance = NAN; _valid = false; return; }
    const uint32_t now = millis();
    if (_waiting) {
        bool complete;
        uint32_t pulse;
        portENTER_CRITICAL(&_mux);
        complete = _complete; pulse = _pulseUs;
        portEXIT_CRITICAL(&_mux);
        if (complete) finish(pulse, nullptr);
        else if (uint32_t(micros() - _triggerUs) >= 30000) finish(0, "timeout de eco");
        return;
    }
    if (now - _lastLog >= 60000) {
        _lastLog = now;
        systemLog.record("SONAR", "leituras=%lu erros=%lu distancia_cm=%.1f angulo=%d erro=%s",
            (unsigned long)_samples, (unsigned long)_errors, _distance, _sampleAngle, _error);
    }
    const bool sweeping = radarServo.sweeping();
    const uint32_t interval = sweeping ? ULTRASONIC_SWEEP_INTERVAL_MS : ULTRASONIC_INTERVAL_MS;
    const uint32_t settle = sweeping ? ULTRASONIC_SWEEP_SETTLE_MS : ULTRASONIC_SETTLE_MS;
    if (now - _lastTrigger < interval) return;
    if (radarServo.enabled() && now - radarServo.lastMoveAt() < settle) return;
    _lastTrigger = now;
    _pendingAngle = radarServo.angle();
    _pendingMoveAt = radarServo.lastMoveAt();
    _pendingAngleKnown = radarServo.enabled();
    // Restart the batch whenever the servo changes position or operating mode.
    if (_batchAngleKnown != _pendingAngleKnown || (_pendingAngleKnown &&
        (_batchMoveAt != _pendingMoveAt || _batchAngle != _pendingAngle))) {
        _echoCount = _validEchoCount = 0;
        _confirmedDistance = _pendingDistance = NAN;
    }
    _batchMoveAt = _pendingMoveAt;
    _batchAngle = _pendingAngle;
    _batchAngleKnown = _pendingAngleKnown;
    if (digitalRead(ULTRASONIC_ECHO_PIN) == HIGH) { finish(0, "ECHO preso em HIGH"); return; }
    portENTER_CRITICAL(&_mux);
    _complete = _riseSeen = false; _armed = true;
    portEXIT_CRITICAL(&_mux);
    _waiting = true;
    _triggerUs = micros();
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(12); // Only the short trigger pulse; never pulseIn()/wait for echo here.
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
}

UltrasonicService::Sample UltrasonicService::snapshot() const {
    Sample s;
    s.ageMs = millis() - _sampleAt;
    s.available = _ready && _enabled && !_paused && _valid && s.ageMs <= 1000;
    s.distanceCm = s.available ? _distance : NAN;
    s.angleKnown = s.available && _angleKnown;
    s.angle = _sampleAngle; s.samples = _samples; s.errors = _errors;
    s.sequence = _sequence; s.servoMoveAt = _sampleMoveAt;
    s.measuredAt = _sampleAt; s.rejectedJumps = _rejectedJumps;
    s.batchCm = _batchDistance; s.positionKnown = _angleKnown; s.error = _error;
    return s;
}

String UltrasonicService::status() const {
    const auto s = snapshot();
    JsonDocument doc;
    doc["model"] = "HC-SR04"; doc["configured"] = _ready; doc["enabled"] = _enabled;
    doc["paused"] = _paused; doc["available"] = s.available; doc["waiting_echo"] = _waiting;
    doc["trig_gpio"] = ULTRASONIC_TRIG_PIN; doc["echo_gpio"] = ULTRASONIC_ECHO_PIN;
    if (s.available) doc["distance_cm"] = s.distanceCm; else doc["distance_cm"] = nullptr;
    if (s.angleKnown) doc["servo_angle"] = s.angle; else doc["servo_angle"] = nullptr;
    doc["angle_is_commanded"] = true; doc["age_ms"] = s.ageMs;
    doc["samples"] = s.samples; doc["errors"] = s.errors; doc["error"] = _error;
    doc["rejected_jumps"] = _rejectedJumps;
    if (isfinite(_batchDistance)) doc["last_batch_cm"] = _batchDistance;
    else doc["last_batch_cm"] = nullptr;
    doc["sequence"] = s.sequence; doc["servo_command_at_ms"] = s.servoMoveAt;
    String json; serializeJson(doc, json); return json + "\n";
}

String UltrasonicService::command(String argument) {
    argument.trim(); argument.toLowerCase();
    if (argument == "on" || argument == "off") {
        cancel(); _echoCount = _validEchoCount = 0; _confirmedDistance = _pendingDistance = NAN; _enabled = argument == "on"; _valid = false;
        systemLog.record("SONAR", "habilitado=%d", _enabled);
    } else if (argument != "status") return "Use ultrasonic:on|off|status\n";
    return status();
}
