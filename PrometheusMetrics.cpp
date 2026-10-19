#include "PrometheusMetrics.h"
#include "PrometheusUi.h"
#include "PortalUi.h"
#include "FirmwareVersion.h"
#include "CpuTemperature.h"
#include "WeatherService.h"
#include "DeviceConfig.h"
#include <Preferences.h>
#include <WiFi.h>
#include <ETH.h>
#include <esp_system.h>
#include "freertos/task.h"
#include "freertos/idf_additions.h"

namespace {
String label(const String& value) {
    String out;
    for (char c : value) {
        if (c == '\\' || c == '"') out += '\\';
        if (c == '\n') out += "\\n";
        else if (c != '\r') out += c;
    }
    return out;
}
bool portNumber(const String& text, uint16_t& port) {
    if (text.isEmpty() || text.length() > 5) return false;
    uint32_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + c - '0';
    }
    if (!value || value > 65535) return false;
    port = value;
    return true;
}
bool validHost(const String& host) {
    if (host.length() > 253) return false;
    if (host.isEmpty()) return false;
    size_t length = 0;
    char before = '.';
    for (char c : host) {
        if (c == '.') {
            if (!length || before == '-') return false;
            length = 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-')) return false;
            if ((!length && c == '-') || ++length > 63) return false;
        }
        before = c;
    }
    return length && before != '-';
}
String integer(uint64_t value) {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
    return buffer;
}
String json(const String& value) {
    String out = "\"";
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') { out += '\\'; out += char(c); }
        else if (c < 32) {
            char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped;
        } else out += char(c);
    }
    return out + '"';
}
}

String PrometheusMetrics::scrapeConfiguration() const {
    IPAddress ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP() :
                   ETH.linkUp() ? ETH.localIP() : WiFi.softAPIP();
    return "# Adicione este job em scrape_configs no servidor Prometheus.\n"
           "scrape_configs:\n  - job_name: 'esp32_p4'\n    scrape_interval: " + String(_interval) +
           "s\n    metrics_path: /metrics\n    static_configs:\n      - targets: ['" +
           ip.toString() + ":" + String(_port) + "']\n";
}

String PrometheusMetrics::configurationJson() const {
    return "{\"host\":" + json(_host) + ",\"port\":" + String(_port) +
           ",\"active_port\":" + String(_activePort) + ",\"web_port\":80,\"interval\":" +
           String(_interval) + ",\"mode\":\"pull\",\"scrape_config\":" + json(scrapeConfiguration()) +
           ",\"persistent_counters_ok\":" + String(_countersSaved ? "true" : "false") + "}";
}

void PrometheusMetrics::begin() {
    Preferences prefs;
    if (prefs.begin("prometheus", false)) {
        String saved = prefs.getString("config", "");
        int a = saved.indexOf('|'), b = saved.indexOf('|', a + 1), c = saved.indexOf('|', b + 1);
        uint16_t port = 80, interval = 15;
        // Version 2: 2|exporter port|interval|host. Also read the previous host/port format.
        bool current = a == 1 && saved[0] == '2';
        bool legacy = a == 1 && (saved[0] == '0' || saved[0] == '1');
        String host = saved.substring(c + 1);
        if (b > a && c > b && (current || legacy) &&
            portNumber(saved.substring(a + 1, b), port) && port != 3232 &&
            (!current || (portNumber(saved.substring(b + 1, c), interval) && interval >= 5 && interval <= 3600)) &&
            (host.isEmpty() || validHost(host))) {
            _port = port; _interval = current ? interval : 15;
            _host = saved.substring(c + 1);
        }
        _boots = prefs.getULong64("boots", 0) + 1;
        esp_reset_reason_t reason = esp_reset_reason();
        _reboots = prefs.getULong64("reboots", 0) + (reason != ESP_RST_POWERON);
        _watchdogs = prefs.getULong64("watchdogs", 0) +
                     (reason == ESP_RST_WDT || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT);
        bool ok = prefs.putULong64("boots", _boots) == sizeof(uint64_t);
        ok = (prefs.putULong64("reboots", _reboots) == sizeof(uint64_t)) && ok;
        ok = (prefs.putULong64("watchdogs", _watchdogs) == sizeof(uint64_t)) && ok;
        _countersSaved = ok;
        prefs.end();
    }
    _server.on("/metrics", HTTP_GET, [this]() { metrics(_server); });
    _server.onNotFound([this]() { _server.send(404, "text/plain", "Use GET /metrics\n"); });
    _server.addMiddleware([this](WebServer& server, Middleware::Callback next) {
        uint64_t started = esp_timer_get_time();
        bool result = next();
        recordRequest(started, server.responseCode() >= 400);
        return result;
    });
    // Bind only from the application loop, after Wi-Fi/AP/Ethernet initialization.
    _pending = true;
}

bool PrometheusMetrics::resetCounters() {
    Preferences prefs;
    if (!prefs.begin("prometheus", false)) {
        _countersSaved = false;
        return false;
    }
    bool ok = prefs.putULong64("boots", 0) == sizeof(uint64_t);
    ok = (prefs.putULong64("reboots", 0) == sizeof(uint64_t)) && ok;
    ok = (prefs.putULong64("watchdogs", 0) == sizeof(uint64_t)) && ok;
    prefs.end();
    _countersSaved = ok;
    if (ok) {
        _boots = 0;
        _reboots = 0;
        _watchdogs = 0;
    }
    return ok;
}

bool PrometheusMetrics::save(const String& rawHost, const String& portText,
                             const String& intervalText, String& error, int& status) {
    status = 400;
    String host = rawHost; host.trim();
    uint16_t port, interval;
    if (!validHost(host)) { error = "Use um hostname ou IPv4, sem http://, caminho ou porta."; return false; }
    if (!portNumber(portText, port) || port == 3232) {
        error = "Porta de métricas inválida. Use 1–65535, exceto 3232 (OTA)."; return false;
    }
    if (!portNumber(intervalText, interval) || interval < 5 || interval > 3600) {
        error = "Use um intervalo de coleta entre 5 e 3600 segundos."; return false;
    }
    status = 503;
    Preferences prefs;
    if (!prefs.begin("prometheus", false)) { error = "Não foi possível abrir as preferências."; return false; }
    String next = "2|" + String(port) + "|" + String(interval) + "|" + host;
    String previous = prefs.getString("config", "");
    // One NVS value keeps host, port and interval consistent across power loss.
    bool ok = prefs.putString("config", next) == next.length() && prefs.getString("config") == next;
    if (!ok) {
        prefs.putString("config", previous);
        error = "Falha ao gravar/verificar as preferências. Confira a configuração antes de repetir.";
    }
    prefs.end();
    if (!ok) return false;
    _host = host; _port = port; _interval = interval;
    _pending = _port != _activePort; _savedAt = millis();
    status = 200;
    return true;
}

void PrometheusMetrics::apply() {
    _server.stop();
    _activePort = _port;
    if (_activePort != 80) _server.begin(_activePort);
    _pending = false;
}

void PrometheusMetrics::recordRequest(uint64_t startedUs, bool error) {
    ++_requests;
    if (error) ++_errors;
    _responseMs = (esp_timer_get_time() - startedUs) / 1000.0;
}

void PrometheusMetrics::sampleCpu() {
#if configGENERATE_RUN_TIME_STATS && !CONFIG_FREERTOS_SMP && CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER
    uint64_t now = esp_timer_get_time();
    if (_cpuAt && now - _cpuAt < 1000000) return;
    double idle = 0;
    for (int core = 0; core < portNUM_PROCESSORS; ++core) {
        configRUN_TIME_COUNTER_TYPE counter = ulTaskGetIdleRunTimeCounterForCore(core);
        if (_cpuAt) idle += configRUN_TIME_COUNTER_TYPE(counter - _idle[core]);
        _idle[core] = counter;
    }
    // Modular subtraction above also handles the SDK's 32-bit counter wrap.
    if (_cpuAt) _cpu = constrain(100.0 - idle * 100.0 / ((now - _cpuAt) * portNUM_PROCESSORS), 0.0, 100.0);
    _cpuAt = now;
#endif
}

void PrometheusMetrics::handle(bool otaBusy) {
    sampleCpu();
    if (_pending && uint32_t(millis() - _savedAt) >= 1000) apply();
    if (_activePort != 80 && !otaBusy) _server.handleClient();
}

void PrometheusMetrics::serveOnWeb(WebServer& server) {
    if (_activePort == 80) metrics(server);
    else server.send(404, "text/plain; charset=utf-8", "Métricas disponíveis na porta configurada do Prometheus.\n");
}

String PrometheusMetrics::page() {
    String out = portalStart("Prometheus", false, false, true);
    out += FPSTR(PROMETHEUS_FORM);
    out += portalEnd();
    return out;
}

void PrometheusMetrics::metrics(WebServer& server) {
    String out;
    if (!out.reserve(16000)) {
        server.send(503, "text/plain", "Memoria insuficiente\n");
        return;
    }
    auto emit = [&](const char* name, const String& value, const String& labels = String(), bool counter = false) {
        out += "# TYPE " + String(name) + (counter ? " counter\n" : " gauge\n");
        out += name; if (labels.length()) out += "{" + labels + "}";
        out += " " + value + "\n";
    };
    auto unavailable = [&](const char* name, const String& labels = String()) { emit(name, "NaN", labels); };
    bool wifi = WiFi.status() == WL_CONNECTED;
    bool eth = ETH.linkUp() && ETH.localIP() != IPAddress(0,0,0,0);
    emit("esp_info", "1", "model=\"" + label(ESP.getChipModel()) + "\",sdk=\"" + label(ESP.getSdkVersion()) + "\"");
    emit("esp_firmware_version", "1", "version=\"" + String(FIRMWARE_VERSION) + "\"");
    out += "# TYPE esp_ip_address gauge\n# TYPE esp_mac_address gauge\n";
    auto identity = [&](const char* interface, IPAddress ip, const String& mac) {
        out += "esp_ip_address{interface=\"" + String(interface) + "\",address=\"" + label(ip.toString()) + "\"} 1\n";
        out += "esp_mac_address{interface=\"" + String(interface) + "\",address=\"" + label(mac) + "\"} 1\n";
    };
    identity("wifi", WiFi.localIP(), WiFi.macAddress());
    identity("ethernet", ETH.localIP(), ETH.macAddress());
    emit("esp_status_online", wifi || eth ? "1" : "0");
    emit("esp_uptime_seconds", String(esp_timer_get_time() / 1000000.0, 3));
    emit("esp_last_restart_reason", String(int(esp_reset_reason())));
    emit("esp_boot_count", _boots ? integer(_boots) : "NaN", "", true);
    emit("esp_reboot_count", _boots ? integer(_reboots) : "NaN", "", true);
    emit("esp_watchdog_resets", _boots ? integer(_watchdogs) : "NaN", "", true);
    emit("esp_prefs_counters_persisted", _countersSaved ? "1" : "0");
    emit("esp_wifi_rssi_dbm", wifi ? String(WiFi.RSSI()) : "NaN");
    emit("esp_wifi_channel", wifi ? String(WiFi.channel()) : "NaN");
    emit("esp_wifi_disconnect_count", integer(_disconnects.load()), "", true);
    // The enum stores signed quarter-dBm values, not whole dBm.
    emit("esp_wifi_tx_power", wifi ? String(int(WiFi.getTxPower()) / 4.0, 2) : "NaN");
    emit("esp_cpu_frequency_mhz", String(ESP.getCpuFreqMHz()));
    emit("esp_cpu_usage_percent", isnan(_cpu) ? "NaN" : String(_cpu, 2));
    float cpuTemperature = readCpuTemperatureCelsius();
    out += "# HELP esp_cpu_temperature_celsius Internal CPU temperature in degrees Celsius.\n";
    emit("esp_cpu_temperature_celsius", isfinite(cpuTemperature) ? String(cpuTemperature, 2) : "NaN");
    emit("esp_task_count", String(uxTaskGetNumberOfTasks()));
    emit("esp_free_rtos_tasks", String(uxTaskGetNumberOfTasks()));
    emit("esp_heap_free_bytes", String(ESP.getFreeHeap()));
    emit("esp_heap_total_bytes", String(ESP.getHeapSize()));
    emit("esp_heap_minimum_free_bytes", String(ESP.getMinFreeHeap()));
    emit("esp_psram_free_bytes", String(ESP.getFreePsram()));
    emit("esp_psram_total_bytes", String(ESP.getPsramSize()));
    uint32_t flash = ESP.getFlashChipSize();
    emit("esp_flash_usage_percent", flash ? String(100.0 * ESP.getSketchSize() / flash, 3) : "NaN");
    const auto climate = weather.snapshot();
    const String weatherLabels = "source=\"open_meteo\",location=\"porto_alegre\"";
    emit("sensor_temperature_celsius", climate.available ? String(climate.temperature, 2) : "NaN", weatherLabels);
    emit("sensor_humidity_percent", climate.available && isfinite(climate.humidity) ? String(climate.humidity, 1) : "NaN", weatherLabels);
    emit("esp_weather_data_age_seconds", isfinite(climate.ageSeconds) ? String(climate.ageSeconds, 0) : "NaN", weatherLabels);
    emit("esp_weather_last_http_status", String(climate.httpStatus), weatherLabels);
    emit("esp_weather_last_request_success", climate.attempted ? (climate.lastRequestOk ? "1" : "0") : "NaN", weatherLabels);
    for (const char* name : {"esp_voltage_v", "esp_current_ma", "esp_power_w", "esp_battery_percent",
                            "esp_battery_voltage_v",
                            "sensor_pressure_hpa", "sensor_air_quality", "relay_state", "relay_activation_count",
                            "relay_runtime_seconds", "gpio_analog_value", "mqtt_messages_sent",
                            "mqtt_messages_received", "mqtt_publish_errors"}) unavailable(name);
    emit("mqtt_connected", "0");
    emit("gpio_digital_input", String(digitalRead(BOTAO_RESET)), "pin=\"" + String(BOTAO_RESET) + "\"");
    emit("gpio_digital_output", String(digitalRead(LED_PIN)), "pin=\"" + String(LED_PIN) + "\"");
    emit("application_requests_total", integer(_requests), "", true);
    emit("application_errors_total", integer(_errors), "", true);
    emit("application_response_time_ms", String(_responseMs, 3));
    const auto& sd = _storage.telemetry();
    emit("sdcard_mounted", sd.mounted ? "1" : "0");
    emit("sdcard_capacity_bytes", sd.mounted ? integer(sd.capacity) : "NaN");
    emit("sdcard_total_bytes", sd.mounted ? integer(sd.total) : "NaN");
    emit("sdcard_used_bytes", sd.mounted ? integer(sd.used) : "NaN");
    emit("sdcard_free_bytes", sd.mounted ? integer(sd.total - sd.used) : "NaN");
    emit("sdcard_usage_percent", sd.mounted && sd.total ? String(100.0 * sd.used / sd.total, 3) : "NaN");
    emit("sdcard_log_size_bytes", sd.mounted && sd.logOk ? integer(sd.logSize) : "NaN");
    emit("sdcard_log_exists", sd.mounted ? (sd.logExists ? "1" : "0") : "NaN");
    emit("sdcard_log_writes_total", integer(sd.logWrites), "", true);
    emit("sdcard_log_errors_total", integer(sd.logErrors), "", true);
    emit("sdcard_crash_reports_total", integer(sd.crashes), "", true);
    emit("sdcard_crash_errors_total", integer(sd.crashErrors), "", true);
    emit("sdcard_last_sample_uptime_seconds", String(sd.sampledAt / 1000000.0, 3));
    out += "# TYPE esp_metric_available gauge\n";
    for (const char* group : {"energy", "relays", "analog", "mqtt"})
        out += "esp_metric_available{group=\"" + String(group) + "\"} 0\n";
    out += String("esp_metric_available{group=\"environment\"} ") + (climate.available ? "1\n" : "0\n");
    out += String("esp_metric_available{group=\"cpu_usage\"} ") + (isnan(_cpu) ? "0\n" : "1\n");
    out += String("esp_metric_available{group=\"cpu_temperature\"} ") + (isfinite(cpuTemperature) ? "1\n" : "0\n");
    out += String("esp_metric_available{group=\"sdcard\"} ") + (sd.mounted ? "1\n" : "0\n");
    out += String("esp_metric_available{group=\"wifi\"} ") + (wifi ? "1\n" : "0\n");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/plain; version=0.0.4; charset=utf-8", out);
}
