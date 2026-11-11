#include "RadarRecording.h"
#include "RadarServo.h"
#include "UltrasonicService.h"
#include <SD_MMC.h>
#include <WebServer.h>
#include <esp_random.h>

RadarRecording radarRecording;

namespace {
constexpr uint32_t DurationMs = 120000;
const char Header[] = "uptime_ms,event,servo_angle_deg,servo_mode,servo_command_ms,target_angle_deg,target_cm,sequence,measurement_ms,measurement_angle_deg,batch_cm,distance_cm,valid,error,rejected_jumps\n";
}

bool RadarRecording::flush() {
    if (!_used) return true;
    File file = SD_MMC.open(_path, FILE_APPEND);
    const bool ok = file && file.write(reinterpret_cast<const uint8_t*>(_buffer), _used) == _used;
    if (file) file.close();
    _lastFlush = millis();
    _used = 0;
    if (!ok) { _failed = true; _active = false; }
    return ok;
}

String RadarRecording::status() const {
    return String("Gravacao: ") + (_active ? "ativa" : _failed ? "falha de escrita; arquivo pode estar parcial" : "parada") +
        "\nArquivo: " + (_path[0] ? _path : "(nenhum)") + "\nLinhas capturadas: " + String(_rows) +
        "\nDownload apos parar: /radar.csv\n";
}

String RadarRecording::command(String arg, bool sdReady) {
    arg.trim(); arg.toLowerCase();
    if (arg == "status") return status();
    if (arg == "off") {
        if (_active) flush();
        _active = false;
        return status();
    }
    if (arg != "on") return "Use radar_log:on|off|status\n";
    if (_active) return status();
    if (!sdReady) return "Cartao SD indisponivel\n";
    char path[sizeof(_path)];
    snprintf(path, sizeof(path), "/radar-%08lx.csv", (unsigned long)esp_random());
    if (SD_MMC.exists(path)) return "Nome ja existe; tente radar_log:on novamente\n";
    File file = SD_MMC.open(path, FILE_WRITE);
    if (!file) return "Nao foi possivel criar o CSV no SD\n";
    const bool ok = file.print(Header) == sizeof(Header) - 1;
    file.close();
    if (!ok) return "Nao foi possivel gravar o cabecalho no SD\n";
    snprintf(_path, sizeof(_path), "%s", path);
    _start = _lastFlush = millis();
    _sequence = ultrasonic.snapshot().sequence;
    _moveAt = radarServo.lastMoveAt();
    _rows = 0; _used = 0; _failed = false; _active = true;
    return status() + "Limite automatico: 120 segundos\n";
}

void RadarRecording::handle(bool paused) {
    if (!_active || paused) return;
    const uint32_t now = millis();
    if (now - _start >= DurationMs) { flush(); _active = false; return; }
    const auto s = ultrasonic.snapshot();
    const bool batch = s.sequence != _sequence;
    const uint32_t moveAt = radarServo.lastMoveAt();
    if (batch || moveAt != _moveAt || !_rows) {
        _sequence = s.sequence; _moveAt = moveAt;
        char details[128] = ",,,,,,,";
        if (batch) {
            char raw[20]{}, valid[20]{}, angle[8]{};
            if (isfinite(s.batchCm)) snprintf(raw, sizeof(raw), "%.3f", s.batchCm);
            if (s.available) snprintf(valid, sizeof(valid), "%.3f", s.distanceCm);
            if (s.positionKnown) snprintf(angle, sizeof(angle), "%d", s.angle);
            snprintf(details, sizeof(details), "%lu,%lu,%s,%s,%s,%d,%s,%lu",
                (unsigned long)s.sequence, (unsigned long)s.measuredAt, angle, raw, valid, int(s.available),
                s.error, (unsigned long)s.rejectedJumps);
        }
        char targetAngle[8]{}, targetCm[20]{};
        if (radarServo.targetLocked()) {
            snprintf(targetAngle, sizeof(targetAngle), "%d", radarServo.trackedAngle());
            snprintf(targetCm, sizeof(targetCm), "%.3f", radarServo.trackedDistance());
        }
        char line[240];
        const int size = snprintf(line, sizeof(line), "%lu,%s,%d,%s,%lu,%s,%s,%s\n",
            (unsigned long)now, batch ? "measurement" : "position", radarServo.angle(),
            radarServo.mode(), (unsigned long)moveAt, targetAngle, targetCm, details);
        if (size <= 0 || size >= int(sizeof(line))) { flush(); _active = false; _failed = true; return; }
        if (_used + size > sizeof(_buffer) && !flush()) return;
        memcpy(_buffer + _used, line, size);
        _used += size;
        ++_rows;
    }
    if (now - _lastFlush >= 2000) flush();
}

void RadarRecording::download(WebServer& server, bool sdReady) {
    if (_active) { server.send(409, "text/plain", "Pare a gravacao com radar_log:off"); return; }
    File file;
    if (sdReady && _path[0]) file = SD_MMC.open(_path, FILE_READ);
    if (!file || file.isDirectory()) { server.send(404, "text/plain", "CSV da sessao indisponivel"); return; }
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Content-Disposition", String("attachment; filename=\"") + (_path + 1) + "\"");
    server.streamFile(file, "text/csv");
    file.close();
}
