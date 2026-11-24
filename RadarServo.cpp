#include "RadarServo.h"
#include "DeviceConfig.h"
#include "SystemLog.h"
#include "UltrasonicService.h"
#include <esp32-hal-ledc.h>

RadarServo radarServo;

void RadarServo::begin()
{
    // Allocate a free LEDC channel; no additional servo library or timer task.
    _ready = ledcAttach(SERVO_PIN, 50, 14);
    if (_ready && !ledcWrite(SERVO_PIN, 0))
        _ready = false;
    systemLog.record("SERVO", "GPIO%d PWM=%d; inicia desligado", SERVO_PIN, _ready);
}

bool RadarServo::writeAngle()
{
    // Conservative 1–2 ms pulse envelope. Actual angles depend on the servo model.
    const uint32_t pulseUs = 1000 + (uint32_t(_angle) * 1000 + 90) / 180;
    const uint32_t duty = (pulseUs * 16384 + 10000) / 20000;
    if (ledcWrite(SERVO_PIN, duty))
    {
        _lastMoveAt = millis();
        return true;
    }
    ledcWrite(SERVO_PIN, 0);
    _ready = _enabled = false;
    systemLog.record("SERVO", "falha ao atualizar PWM; desativado");
    return false;
}

void RadarServo::handle(bool paused)
{
    if (!_ready)
        return;
    if (paused != _paused)
    {
        _paused = paused;
        _lastStep = millis();
        if (_paused)
            ledcWrite(SERVO_PIN, 0);
        else if (_enabled)
            writeAngle();
    }
    if (_paused || !_enabled)
        return;
    const uint32_t interval = _sweep ? RADAR_SWEEP_INTERVAL_MS : SERVO_STEP_MS;
    if (millis() - _lastStep < interval)
        return;
    // Finish a pending echo before changing position, without waiting for a valid result.
    if (ultrasonic.busy())
        return;
    _lastStep = millis(); // One step only; do not catch up in a burst after a delay.
    if (_sweep)
    {
        if (_angle >= SERVO_MAX_ANGLE)
            _direction = -1;
        else if (_angle <= SERVO_MIN_ANGLE)
            _direction = 1;
        _angle = constrain(_angle + _direction * SERVO_SWEEP_STEP_DEGREES, SERVO_MIN_ANGLE,
                           SERVO_MAX_ANGLE);
    }
    else if (_angle != _target)
    {
        _angle += _angle < _target ? 1 : -1;
    }
    else
        return;
    writeAngle();
}

String RadarServo::status() const
{
    return String("Servo GPIO") + SERVO_PIN +
           " (P6 pino 21)\nPWM: " + (_ready ? "pronto" : "falha") + "\nEstado: " + mode() +
           "\nAngulo comandado (sem sensor): " + String(_angle) +
           "\nAlvo fixo: " + String(_target) + "\nFaixa: " + String(SERVO_MIN_ANGLE) + ".." +
           String(SERVO_MAX_ANGLE) + " graus nominais\nRadar: " + String(SERVO_SWEEP_STEP_DEGREES) +
           " graus / " + String(RADAR_SWEEP_INTERVAL_MS) +
           " ms\nSincronismo: temporizador independente do sensor e do video" +
           "\nRastreamento de alvos: removido\n";
}

String RadarServo::command(String argument)
{
    argument.trim();
    argument.toLowerCase();
    if (argument == "status")
        return status();
    if (argument.startsWith("rastrear:"))
        return "Rastreamento removido; use servo:on para varrer o ambiente\n";
    if (argument == "off")
    {
        if (_ready)
            ledcWrite(SERVO_PIN, 0);
        _enabled = false;

        systemLog.record("SERVO", "desligado; pulsos removidos");
        return status();
    }
    int target = _angle;
    const bool sweep = argument == "on";
    if (!sweep)
    {
        if (!argument.startsWith("angulo:"))
            return "Use servo:on|off|status|angulo:5..175\n";
        const String number = argument.substring(7);
        if (number.isEmpty() || number.length() > 3)
            return "Angulo invalido: use 5..175\n";
        for (char c : number)
            if (c < '0' || c > '9')
                return "Angulo invalido: use 5..175\n";
        target = number.toInt();
        if (target < SERVO_MIN_ANGLE || target > SERVO_MAX_ANGLE)
            return "Angulo invalido: use 5..175\n";
    }
    if (!_ready)
        return "PWM indisponivel; consulte servo:status\n";
    if (_paused)
        return "Servo pausado durante OTA\n";
    _sweep = sweep;

    _target = target;
    _enabled = true;
    _lastStep = millis();

    writeAngle();
    systemLog.record("SERVO", "modo=%s alvo=%d", sweep ? "radar" : "fixo", target);
    return status();
}
