#include "SystemLog.h"
#include "StorageManager.h"
#include <ArduinoJson.h>
#include <esp_timer.h>
#include <stdarg.h>

SystemLog systemLog;

String SystemLog::sanitize(const String& text) {
    String out = text.substring(0, 512), lower = out; lower.toLowerCase();
    for (auto word : {"password", "senha", "authorization", "wifi_add:", "ota_password", "token="})
        if (lower.indexOf(word) >= 0) return "[conteudo sensivel omitido]";
    String clean; clean.reserve(out.length());
    for (size_t i = 0; i < out.length(); ++i) {
        uint8_t c = out[i];
        if (c == 27 && i + 1 < out.length() && out[i+1] == '[') {
            i += 2; while (i < out.length() && !(out[i] >= '@' && out[i] <= '~')) ++i;
        } else clean += c < 32 || c == 127 ? ' ' : char(c);
    }
    return clean;
}

void SystemLog::begin() {
    if (_queue) return;
    _queue = xQueueCreate(64, sizeof(Entry));
    if (_queue) _previous = esp_log_set_vprintf(capture);
    record("BOOT", "log iniciado; fila=%s", _queue ? "64" : "indisponivel");
}

void SystemLog::record(const char* category, const char* format, ...) {
    if (!_queue) { ++_dropped; return; }
    Entry entry = {}; entry.uptime = esp_timer_get_time() / 1000000;
    strlcpy(entry.category, category, sizeof(entry.category));
    va_list args; va_start(args, format); vsnprintf(entry.message, sizeof(entry.message), format, args); va_end(args);
    if (xQueueSend(_queue, &entry, 0) != pdTRUE) ++_dropped;
}

int SystemLog::capture(const char* format, va_list args) {
    va_list output; va_copy(output, args);
    int result = systemLog._previous ? systemLog._previous(format, output) : vprintf(format, output);
    va_end(output);
    // Do not turn an SD driver write failure into an endless log-write recursion.
    if (!systemLog._writing && !systemLog._hookBusy.test_and_set()) {
        char text[200]; va_list copy; va_copy(copy, args); vsnprintf(text, sizeof(text), format, copy); va_end(copy);
        // SDK warnings/errors only: debug dumps can contain HID payloads and overload the card.
        if (strstr(text, "E (") || strstr(text, "W (") || strstr(text, "[E]") || strstr(text, "[W]"))
            systemLog.record("SDK", "%s", text);
        systemLog._hookBusy.clear();
    }
    return result;
}

void SystemLog::process(StorageManager& storage, bool busy) {
    if (busy || !_queue || !storage.isAtivo() || int32_t(millis() - _nextFlush) < 0) return;
    _nextFlush = millis() + 1000;
    Entry entry;
    for (unsigned i = 0; i < 4 && xQueueReceive(_queue, &entry, 0) == pdTRUE; ++i) {
        String line = "[up=" + String((unsigned long)entry.uptime) + "s][" + String(entry.category) + "] " + sanitize(entry.message);
        uint64_t before = storage.telemetry().logWrites;
        _writing = true; storage.gravarLog(line); _writing = false;
        if (storage.telemetry().logWrites > before) ++_written; else ++_dropped;
    }
}

String SystemLog::status() const {
    JsonDocument doc;
    doc["ready"] = _queue != nullptr; doc["queued"] = _queue ? uxQueueMessagesWaiting(_queue) : 0;
    doc["written"] = written(); doc["dropped"] = dropped();
    doc["file"] = "/log.txt"; doc["rotation_bytes"] = 1048576; doc["archives"] = 4;
    String json; serializeJson(doc, json); return json;
}
