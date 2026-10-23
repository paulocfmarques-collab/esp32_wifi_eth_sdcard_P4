#include "EdgeComputing.h"
#include "PortalUi.h"
#include "CpuTemperature.h"
#include "PageAudio.h"
#include "MqttService.h"
#include <Preferences.h>
#include <esp_timer.h>

EdgeComputing edge;
namespace {
String serialize(const JsonDocument& doc) { String text; serializeJson(doc, text); return text; }
bool number(WebServer& server, const char* key, unsigned minimum, unsigned maximum, unsigned& result) {
    String value = server.arg(key);
    if (value.isEmpty() || value.length() > 8) return false;
    for (size_t i = 0; i < value.length(); ++i) if (!isDigit(value[i])) return false;
    unsigned long parsed = strtoul(value.c_str(), nullptr, 10);
    if (parsed < minimum || parsed > maximum) return false;
    result = parsed; return true;
}
}

const char* EdgeComputing::ruleName(unsigned i) {
    static const char* names[] = {"cpu_hot", "heap_low", "network_down", "udp_errors"};
    return i < RULE_COUNT ? names[i] : "unknown";
}

void EdgeComputing::encodeConfig(JsonDocument& doc, const Config& config) {
    doc["enabled"] = config.enabled; doc["sound"] = config.sound;
    doc["cpu_limit"] = config.cpuLimit; doc["heap_limit"] = config.heapLimit;
    doc["udp_limit"] = config.udpLimit; doc["confirm_seconds"] = config.confirmSeconds;
    doc["cooldown_seconds"] = config.cooldownSeconds;
}

void EdgeComputing::begin() {
    Preferences prefs;
    if (prefs.begin("edge", true)) {
        JsonDocument doc;
        if (!deserializeJson(doc, prefs.getString("config", "{}"))) {
            Config next;
            next.enabled = doc["enabled"] | false; next.sound = doc["sound"] | false;
            next.cpuLimit = doc["cpu_limit"] | 70U; next.heapLimit = doc["heap_limit"] | 65536U;
            next.udpLimit = doc["udp_limit"] | 5U; next.confirmSeconds = doc["confirm_seconds"] | 10U;
            next.cooldownSeconds = doc["cooldown_seconds"] | 300U;
            if (next.cpuLimit >= 20 && next.cpuLimit <= 100 && next.heapLimit >= 8192 && next.heapLimit <= 1048576 &&
                next.udpLimit >= 1 && next.udpLimit <= 10000 && next.confirmSeconds >= 1 && next.confirmSeconds <= 600 &&
                next.cooldownSeconds >= 30 && next.cooldownSeconds <= 86400) _config = next;
        }
        prefs.end();
    }
    resetWindow();
}

void EdgeComputing::resetWindow() {
    _baseline = false; _udpWindow = 0;
    memset(_udpBuckets, 0, sizeof(_udpBuckets));
    _started = esp_timer_get_time(); _nextSample = 0;
    for (auto& rule : _rules) { rule.pending = false; rule.valid = false; }
}

void EdgeComputing::evaluate(unsigned i, bool bad, bool valid, uint64_t now, StorageManager& storage) {
    Rule& rule = _rules[i]; rule.valid = valid;
    if (!valid) { rule.pending = false; return; }
    if (bad != rule.active) {
        if (!rule.pending || rule.candidate != bad) {
            rule.candidate = bad; rule.pending = true; rule.since = now;
        }
        if (now - rule.since >= uint64_t(_config.confirmSeconds) * 1000000) {
            rule.active = bad; rule.pending = false;
            if (!bad || !rule.alerts || now - rule.notifiedAt >= uint64_t(_config.cooldownSeconds) * 1000000)
                notify(i, bad, now, storage);
        }
    } else {
        rule.pending = false;
        if (bad && now - rule.notifiedAt >= uint64_t(_config.cooldownSeconds) * 1000000)
            notify(i, true, now, storage);
    }
}

void EdgeComputing::notify(unsigned i, bool active, uint64_t now, StorageManager& storage) {
    if (active) { ++_rules[i].alerts; _rules[i].notifiedAt = now; }
    Event event; event.sequence = ++_events; event.uptime = now / 1000000; event.rule = i; event.active = active;
    _history[_historyNext] = event; _historyNext = (_historyNext + 1) % 16;
    String message = String("[EDGE] ") + ruleName(i) + (active ? " ALERTA" : " RECUPERADO");
    Serial.println(message);
    storage.gravarLog(message);
    mqtt.edgeEvent(ruleName(i), active, event.sequence, event.uptime);
    if (active && _config.sound) pageAudio.play(true);
}

void EdgeComputing::handle(StorageManager& storage, bool busy, bool networkUp, uint64_t udpErrors) {
    if (busy) { _paused = true; return; }
    if (_paused) { resetWindow(); _paused = false; }
    if (!_config.enabled) return;
    uint64_t now = esp_timer_get_time();
    if (now < _nextSample) return;
    _nextSample = now + 1000000;
    _cpu = readCpuTemperatureCelsius(); _heap = ESP.getFreeHeap(); _network = networkUp;
    uint64_t second = now / 1000000;
    if (!_baseline) { _lastUdp = udpErrors; _udpSecond = second; _baseline = true; }
    uint64_t elapsed = second - _udpSecond;
    if (elapsed >= 60) { memset(_udpBuckets, 0, sizeof(_udpBuckets)); _udpWindow = 0; _started = now; }
    else for (uint64_t s = _udpSecond + 1; s <= second; ++s) {
        _udpWindow -= _udpBuckets[s % 60]; _udpBuckets[s % 60] = 0;
    }
    // A counter reset establishes a new baseline rather than creating an unsigned spike.
    uint64_t delta = elapsed < 60 && udpErrors >= _lastUdp ? udpErrors - _lastUdp : 0;
    _udpBuckets[second % 60] += delta; _udpWindow += delta;
    _lastUdp = udpErrors; _udpSecond = second;
    bool warm = now - _started >= 60000000;
    evaluate(0, _rules[0].active ? _cpu > float(_config.cpuLimit) - 3 : _cpu >= _config.cpuLimit, isfinite(_cpu), now, storage);
    evaluate(1, _rules[1].active ? uint64_t(_heap) < uint64_t(_config.heapLimit) + 8192 : _heap < _config.heapLimit, true, now, storage);
    evaluate(2, !networkUp, warm, now, storage);
    evaluate(3, _udpWindow >= _config.udpLimit, warm, now, storage);
}

uint32_t EdgeComputing::activeCount() const {
    unsigned count = 0; for (const auto& rule : _rules) if (rule.active) ++count; return count;
}

String EdgeComputing::configuration() { JsonDocument doc; encodeConfig(doc, _config); return serialize(doc); }

String EdgeComputing::status() {
    JsonDocument doc; encodeConfig(doc, _config);
    doc["active_count"] = activeCount(); doc["events_total"] = _events; doc["paused"] = _paused;
    if (_config.enabled && isfinite(_cpu)) doc["cpu"] = _cpu; else doc["cpu"] = nullptr;
    if (_config.enabled) { doc["heap"] = _heap; doc["network_up"] = _network; doc["udp_errors_60s"] = _udpWindow; }
    auto rule = doc["rules"].to<JsonArray>();
    for (unsigned i = 0; i < RULE_COUNT; ++i) {
        auto item = rule.add<JsonObject>();
        item["rule"] = ruleName(i); item["active"] = _rules[i].active;
        item["valid"] = _rules[i].valid; item["pending"] = _rules[i].pending; item["alerts"] = _rules[i].alerts;
    }
    auto history = doc["history"].to<JsonArray>();
    for (unsigned n = 0; n < 16; ++n) {
        const Event& event = _history[(_historyNext + 15 - n) % 16];
        if (!event.sequence) continue;
        auto item = history.add<JsonObject>(); item["sequence"] = event.sequence; item["uptime"] = event.uptime;
        item["rule"] = ruleName(event.rule); item["active"] = event.active;
    }
    return serialize(doc);
}

void EdgeComputing::save(WebServer& server) {
    if (server.header("X-ESP32-Command") != "1") { server.send(400, "text/plain", "Header obrigatorio"); return; }
    Config next;
    String enabled = server.arg("enabled"), sound = server.arg("sound");
    if ((enabled != "0" && enabled != "1") || (sound != "0" && sound != "1") ||
        !number(server, "cpu_limit", 20, 100, next.cpuLimit) ||
        !number(server, "heap_limit", 8192, 1048576, next.heapLimit) ||
        !number(server, "udp_limit", 1, 10000, next.udpLimit) ||
        !number(server, "confirm_seconds", 1, 600, next.confirmSeconds) ||
        !number(server, "cooldown_seconds", 30, 86400, next.cooldownSeconds)) {
        server.send(400, "text/plain", "Confira limites e tempos"); return;
    }
    next.enabled = enabled == "1"; next.sound = sound == "1";
    JsonDocument doc; encodeConfig(doc, next); String encoded = serialize(doc);
    Preferences prefs;
    if (!prefs.begin("edge", false)) { server.send(503, "text/plain", "NVS indisponivel"); return; }
    String old = prefs.getString("config", "");
    bool ok = prefs.putString("config", encoded) == encoded.length() && prefs.getString("config", "") == encoded;
    if (!ok) { if (old.isEmpty()) prefs.remove("config"); else prefs.putString("config", old); }
    prefs.end();
    if (!ok) { server.send(503, "text/plain", "Falha ao salvar; confira antes de reiniciar"); return; }
    _config = next;
    for (auto& rule : _rules) { rule.active = false; rule.pending = false; }
    resetWindow();
    mqtt.refreshState();
    server.send(200, "application/json", configuration());
}

void EdgeComputing::routes(WebServer& server, bool (*busy)()) {
    server.on("/edge", HTTP_GET, [this, &server]() { server.sendHeader("Cache-Control", "no-store"); server.send(200, "text/html; charset=utf-8", page()); });
    server.on("/api/edge/status", HTTP_GET, [this, &server]() { server.sendHeader("Cache-Control", "no-store"); server.send(200, "application/json", status()); });
    server.on("/api/edge/config", HTTP_GET, [this, &server]() { server.sendHeader("Cache-Control", "no-store"); server.send(200, "application/json", configuration()); });
    server.on("/api/edge/config", HTTP_POST, [this, &server, busy]() {
        server.sendHeader("Cache-Control", "no-store");
        if (busy && busy()) { server.send(409, "text/plain", "Aguarde OTA ou reinicio"); return; }
        save(server);
    });
}

String EdgeComputing::page() {
    return portalStart("Processamento local", false, false, false, false, true) +
        F("<header class='page-header'><h1>Processamento local</h1></header><section class='card'><p>Regras executadas na placa, mesmo sem internet. Alertas na Serial e no log SD, quando disponível; MQTT envia eventos enquanto conectado.</p>") +
        FPSTR(EDGE_FORM) + F("</section>") + portalEnd();
}
