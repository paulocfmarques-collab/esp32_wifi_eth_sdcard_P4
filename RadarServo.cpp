#include "RadarServo.h"
#include "DeviceConfig.h"
#include "SystemLog.h"
#include "CameraService.h"
#include "UltrasonicService.h"
#include <esp32-hal-ledc.h>

namespace {
constexpr uint32_t SonarStepDeadlineMs = 600;
constexpr uint32_t SonarConfirmationMs = 1200;
constexpr uint32_t SonarProbeDeadlineMs = 1200;
constexpr uint32_t SonarCenterGraceMs = 850;
}

RadarServo radarServo;

void RadarServo::begin() {
    // Allocate a free LEDC channel; no additional servo library or timer task.
    _ready = ledcAttach(SERVO_PIN, 50, 14);
    if (_ready && !ledcWrite(SERVO_PIN, 0)) _ready = false;
    systemLog.record("SERVO", "GPIO%d PWM=%d; inicia desligado", SERVO_PIN, _ready);
}

bool RadarServo::writeAngle() {
    // Conservative 1–2 ms pulse envelope. Actual angles depend on the servo model.
    const uint32_t pulseUs = 1000 + (uint32_t(_angle) * 1000 + 90) / 180;
    const uint32_t duty = (pulseUs * 16384 + 10000) / 20000;
    if (ledcWrite(SERVO_PIN, duty)) { _lastMoveAt = millis(); return true; }
    ledcWrite(SERVO_PIN, 0);
    _ready = _enabled = false;
    systemLog.record("SERVO", "falha ao atualizar PWM; desativado");
    return false;
}

void RadarServo::resetTracking() {
    _locked = false; _confirmations = _probePhase = _lostCycles = 0;
    _trackDistance = _candidateDistance = NAN;
    _bestSearchDistance = NAN;
    _searchEndSeen = _returningToEcho = false;
    _lastSonarSequence = ultrasonic.snapshot().sequence;
}

void RadarServo::moveTo(int angle) {
    _angle = constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    _lastStep = millis();
    writeAngle();
}

void RadarServo::nextSearchStep() {
    if (_trackingEnabled && (_angle <= SERVO_MIN_ANGLE || _angle >= SERVO_MAX_ANGLE)) {
        if (_searchEndSeen && isfinite(_bestSearchDistance)) {
            const int angle = _bestSearchAngle;
            const float distance = _bestSearchDistance;
            resetTracking();
            _returningToEcho = true;
            _confirmations = 1;
            _candidateDistance = distance;
            _candidateAt = millis();
            moveTo(angle);
            return;
        }
        _searchEndSeen = true;
    }
    if (_angle >= SERVO_MAX_ANGLE) _direction = -1;
    else if (_angle <= SERVO_MIN_ANGLE) _direction = 1;
    moveTo(_angle + _direction * RADAR_SEARCH_STEP_DEGREES);
}

void RadarServo::startProbeCycle() {
    const int radius = RADAR_PROBE_DEGREES * (_lostCycles ? 2 : 1);
    _probeAngles[0] = _trackCenter;
    _probeAngles[1] = constrain(_trackCenter - radius, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    _probeAngles[2] = constrain(_trackCenter + radius, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    for (auto& valid : _probeValid) valid = false;
    _probePhase = 0;
    moveTo(_probeAngles[0]);
}

void RadarServo::sonarSweep() {
    if (_locked && millis() - _lastSeenAt >= 2500) {
        systemLog.record("RADAR", "confirmacao do alvo expirou; retomando busca");
        resetTracking();
    }
    const auto sample = ultrasonic.snapshot();
    // Consume each completed attempt once, including timeouts, at the actual commanded position.
    const uint32_t now = millis();
    const bool fresh = sample.sequence && sample.sequence != _lastSonarSequence && sample.ageMs <= 1000 &&
        sample.servoMoveAt == _lastMoveAt && sample.angle == _angle;
    const bool expired = _confirmations ? now - _candidateAt >= SonarConfirmationMs :
        now - _lastStep >= (_locked ? SonarProbeDeadlineMs : SonarStepDeadlineMs);
    if (!fresh && !expired) return;
    if (fresh) _lastSonarSequence = sample.sequence;
    const bool candidate = fresh && sample.available && sample.angleKnown &&
        sample.distanceCm >= RADAR_TARGET_MIN_CM && sample.distanceCm <= RADAR_TARGET_MAX_CM;
    if (!_trackingEnabled) { nextSearchStep(); return; }
    if (!_locked) {
        if (candidate && !_confirmations) {
            _candidateDistance = sample.distanceCm;
            _candidateAt = now;
            _confirmations = 1;
            return;
        }
        if (candidate && _confirmations && !expired && fabsf(sample.distanceCm - _candidateDistance) <= 25.0f) {
            if (!_returningToEcho) {
                if (!isfinite(_bestSearchDistance) || sample.distanceCm < _bestSearchDistance) {
                    _bestSearchDistance = sample.distanceCm;
                    _bestSearchAngle = _angle;
                }
                _confirmations = 0;
                nextSearchStep();
                return;
            }
            _returningToEcho = false;
            _locked = true; _confirmations = 0;
            _trackCenter = _angle; _trackDistance = sample.distanceCm; _lastSeenAt = millis();
            systemLog.record("RADAR", "alvo confirmado angulo=%d distancia_cm=%.1f", _trackCenter, _trackDistance);
            startProbeCycle();
            return;
        }
        if (_confirmations && !expired) return; // A failed batch need not discard a promising direction.
        if (_returningToEcho) resetTracking();
        _confirmations = 0;
        nextSearchStep();
        return;
    }
    const float gate = fmaxf(30.0f, _trackDistance * 0.30f);
    _probeValid[_probePhase] = candidate && fabsf(sample.distanceCm - _trackDistance) <= gate;
    _probeDistances[_probePhase] = sample.distanceCm;
    if (_probeValid[_probePhase]) _lastSeenAt = millis();
    if (_probePhase == 0 && _probeValid[0]) {
        _trackDistance = sample.distanceCm;
        _lostCycles = 0;
        return; // Keep pointing at a stable central echo without lateral oscillation.
    }
    if (_probePhase == 0 && !_lostCycles && now - _lastSeenAt < SonarCenterGraceMs) return;
    if (++_probePhase < 3) { moveTo(_probeAngles[_probePhase]); return; }
    // Prefer the center unless a side echo is clearly closer; reject unrelated depth jumps.
    int best = _probeValid[0] ? 0 : -1;
    for (int i = 1; i < 3; ++i)
        if (_probeValid[i] && (best < 0 || _probeDistances[i] < _probeDistances[best] - 5.0f)) best = i;
    if (best >= 0) {
        _trackCenter = _probeAngles[best]; _trackDistance = _probeDistances[best]; _lostCycles = 0;
    } else ++_lostCycles;
    if (_lostCycles >= 3 || millis() - _lastSeenAt >= 2500) {
        systemLog.record("RADAR", "alvo perdido; retomando busca");
        resetTracking(); nextSearchStep(); return;
    }
    startProbeCycle();
}

void RadarServo::handle(bool paused) {
    if (!_ready) return;
    if (paused != _paused) {
        _paused = paused;
        resetTracking();
        _lastStep = millis();
        if (_paused) ledcWrite(SERVO_PIN, 0);
        else if (_enabled) writeAngle();
    }
    if (_paused || !_enabled) return;
    if (_sweep && ultrasonic.enabled()) {
        if (!ultrasonic.busy()) sonarSweep();
        return;
    }
    if (ultrasonic.busy()) return;
    if (_locked || _confirmations) resetTracking();
    const uint32_t interval = _sweep ? CAMERA_FRAME_INTERVAL_MS : SERVO_STEP_MS;
    if (millis() - _lastStep < interval) return;
    if (_sweep) {
        // Follow the newest successfully transmitted frame, not a free-running motor clock.
        // Without viewers, follow capture; without a camera, keep a slow standalone sweep.
        const bool streaming = camera.streamClients() > 0;
        const uint32_t frame = streaming ? camera.lastStreamFrame() : camera.frames();
        if (streaming || camera.available()) {
            if (frame == _motionFrame) return;
            _motionFrame = frame;
        }
    }
    // One step only, even if the application was delayed: never catch up in a burst.
    _lastStep = millis();
    if (_sweep) {
        if (_angle >= SERVO_MAX_ANGLE) _direction = -1;
        else if (_angle <= SERVO_MIN_ANGLE) _direction = 1;
        _angle = constrain(_angle + _direction * SERVO_SWEEP_STEP_DEGREES, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    } else if (_angle != _target) {
        _angle += _angle < _target ? 1 : -1;
    } else return;
    writeAngle();
}

String RadarServo::status() const {
    return String("Servo GPIO") + SERVO_PIN + " (P6 pino 21)\nPWM: " +
        (_ready ? "pronto" : "falha") + "\nEstado: " +
        mode() +
        "\nAngulo comandado (sem sensor): " + String(_angle) +
        "\nAlvo fixo: " + String(_target) + "\nFaixa: " + String(SERVO_MIN_ANGLE) +
        ".." + String(SERVO_MAX_ANGLE) + " graus nominais\nPasso: 1 grau / " +
        String(SERVO_STEP_MS) + " ms (posicao fixa)\nRadar sem ultrassom: ate " +
        String(SERVO_SWEEP_STEP_DEGREES) + " graus / quadro; minimo " +
        String(CAMERA_FRAME_INTERVAL_MS) + " ms\nSincronismo: " +
        (_sweep && ultrasonic.enabled() ? "leituras do ultrassom" : camera.streamClients() > 0 ? "quadros enviados ao stream" : camera.available() ? "captura local" : "temporizador sem camera") +
        "\nUltimo quadro enviado: " + String(camera.lastStreamFrame()) +
        "\nCom ultrassom: busca " + String(RADAR_SEARCH_STEP_DEGREES) + " graus / leitura concluida; espera maxima " + String(SonarStepDeadlineMs) + " ms" +
        "\nRastreamento: " + (_trackingEnabled ? "habilitado" : "desabilitado") +
        "\nFaixa de deteccao: " + String(RADAR_TARGET_MIN_CM, 0) + ".." + String(RADAR_TARGET_MAX_CM, 0) + " cm" +
        "\nConfirmacao: ate " + String(SonarConfirmationMs) + " ms apos o primeiro lote valido" +
        "\nAlvo: " + (targetLocked() ? String(_trackCenter) + " graus / " + String(_trackDistance, 1) + " cm" : "nenhum") + "\n";
}

String RadarServo::command(String argument) {
    argument.trim();
    argument.toLowerCase();
    if (argument == "status") return status();
    if (argument == "rastrear:on" || argument == "rastrear:off") {
        _trackingEnabled = argument == "rastrear:on";
        resetTracking();
        systemLog.record("RADAR", "rastreamento=%d", _trackingEnabled);
        return status();
    }
    if (argument == "off") {
        if (_ready) ledcWrite(SERVO_PIN, 0);
        _enabled = false;
        resetTracking();
        systemLog.record("SERVO", "desligado; pulsos removidos");
        return status();
    }
    int target = _angle;
    const bool sweep = argument == "on";
    if (!sweep) {
        if (!argument.startsWith("angulo:")) return "Use servo:on|off|status|angulo:5..175|rastrear:on|rastrear:off\n";
        const String number = argument.substring(7);
        if (number.isEmpty() || number.length() > 3) return "Angulo invalido: use 5..175\n";
        for (char c : number) if (c < '0' || c > '9') return "Angulo invalido: use 5..175\n";
        target = number.toInt();
        if (target < SERVO_MIN_ANGLE || target > SERVO_MAX_ANGLE) return "Angulo invalido: use 5..175\n";
    }
    if (!_ready) return "PWM indisponivel; consulte servo:status\n";
    if (_paused) return "Servo pausado durante OTA\n";
    _sweep = sweep;
    resetTracking();
    _target = target;
    _enabled = true;
    _lastStep = millis();
    _motionFrame = camera.streamClients() > 0 ? camera.lastStreamFrame() : camera.frames();
    writeAngle();
    systemLog.record("SERVO", "modo=%s alvo=%d", sweep ? "radar" : "fixo", target);
    return status();
}
