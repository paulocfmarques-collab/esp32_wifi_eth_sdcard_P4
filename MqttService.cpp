#include "MqttService.h"
#include "PortalUi.h"
#include "DeviceConfig.h"
#include "FirmwareVersion.h"
#include "WeatherService.h"
#include "CpuTemperature.h"
#include "EdgeComputing.h"
#include "SystemLog.h"
#include <WiFi.h>
#include <ETH.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <ctype.h>

MqttService mqtt;
namespace {
String jsonText(const JsonDocument& doc) { String out; serializeJson(doc, out); return out; }
bool validHost(const String& host) {
    if (host.isEmpty() || host.length() > 253) return false;
    for (size_t i = 0; i < host.length(); ++i) {
        char c = host[i];
        if (!isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-') return false;
    }
    return true;
}
struct Entity { const char *key, *name, *unit, *deviceClass; };
const Entity sensors[] = {
    {"cpu_temperature", "Temperatura CPU", "°C", "temperature"},
    {"temperature", "Temperatura externa Porto Alegre", "°C", "temperature"},
    {"humidity", "Umidade externa Porto Alegre", "%", "humidity"},
    {"uptime", "Tempo ligado", "s", "duration"},
    {"heap_free", "Memoria livre", "B", "data_size"},
    {"wifi_rssi", "Sinal Wi-Fi", "dBm", "signal_strength"},
    {"sd_free", "SD livre", "B", "data_size"},
    {"sd_used", "SD usado", "B", "data_size"}
};
const char* actions[] = {"tela:next", "tela:auto", "protetor:on", "protetor:off", "audio_test", "camera_oled"};
const char* actionNames[] = {"Proxima tela", "Telas automaticas", "Carinhas ligar", "Carinhas desligar", "Testar som", "Camera no OLED"};
constexpr size_t SENSOR_COUNT = sizeof(sensors) / sizeof(sensors[0]);
constexpr size_t ACTION_COUNT = sizeof(actions) / sizeof(actions[0]);
}

void MqttService::begin() {
    char suffix[18]; snprintf(suffix, sizeof(suffix), "%012llx", (unsigned long long)ESP.getEfuseMac());
    _id = String("esp32p4_") + suffix; _base = "esp32/" + _id;
    _commands = xQueueCreate(8, sizeof(Command));
    Preferences prefs;
    if (prefs.begin("mqtt", true)) {
        JsonDocument doc;
        if (!deserializeJson(doc, prefs.getString("config", "{}"))) {
            _config.enabled = doc["enabled"] | false;
            _config.host = doc["host"] | ""; _config.port = doc["port"] | 1883;
            _config.user = doc["user"] | ""; _config.password = doc["password"] | "";
        }
        prefs.end();
    }
    if (!validHost(_config.host) || !_config.port) _config.enabled = false;
}

void MqttService::start() {
    esp_mqtt_client_config_t config = {};
    config.broker.address.hostname = _config.host.c_str();
    config.broker.address.port = _config.port;
    config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    config.credentials.client_id = _id.c_str();
    config.credentials.username = _config.user.isEmpty() ? nullptr : _config.user.c_str();
    config.credentials.authentication.password = _config.password.isEmpty() ? nullptr : _config.password.c_str();
    String availability = _base + "/availability";
    config.session.last_will.topic = availability.c_str();
    config.session.last_will.msg = "offline";
    config.session.last_will.qos = 1; config.session.last_will.retain = 1;
    config.session.keepalive = 30;
    config.network.reconnect_timeout_ms = 10000;
    config.network.timeout_ms = 5000;
    config.buffer.size = 2048;
    config.task.stack_size = 8192;
    config.outbox.limit = 16384;
    _client = esp_mqtt_client_init(&config);
    if (!_client) { ++_errors; return; }
    if (esp_mqtt_client_register_event(_client, MQTT_EVENT_ANY, event, this) != ESP_OK ||
        esp_mqtt_client_start(_client) != ESP_OK) { ++_errors; stop(); }
}

void MqttService::stop() {
    if (!_client) return;
    // Configuration changes only; MQTT callbacks never touch hardware or stop the client.
    if (_connected) esp_mqtt_client_publish(_client, (_base + "/availability").c_str(), "offline", 0, 1, 1);
    esp_mqtt_client_stop(_client);
    esp_mqtt_client_destroy(_client); _client = nullptr;
    _connected = false; _announce = false; _discovering = false;
    if (_commands) xQueueReset(_commands);
}

void MqttService::event(void* arg, esp_event_base_t, int32_t id, void* data) {
    auto& self = *static_cast<MqttService*>(arg);
    auto e = static_cast<esp_mqtt_event_handle_t>(data);
    if (id == MQTT_EVENT_CONNECTED) {
        systemLog.record("MQTT", "conectado ao broker");
        self._connected = true; self._announce = true;
        self._errorType = 0; self._brokerCode = 0; self._socketError = 0; self._transportError = 0;
    }
    else if (id == MQTT_EVENT_DISCONNECTED) { self._connected = false; ++self._disconnects; systemLog.record("MQTT", "desconectado; reconexao automatica"); }
    else if (id == MQTT_EVENT_PUBLISHED) ++self._sent;
    else if (id == MQTT_EVENT_ERROR) {
        ++self._errors;
        if (e->error_handle) {
            const auto& error = *e->error_handle;
            self._errorType = error.error_type;
            self._brokerCode = error.error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED ? error.connect_return_code : 0;
            self._socketError = error.error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT ? error.esp_transport_sock_errno : 0;
            self._transportError = error.error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT ? error.esp_tls_last_esp_err : 0;
            systemLog.record("MQTT", "erro tipo=%d broker=%d socket=%d transporte=%d", self._errorType.load(), self._brokerCode.load(), self._socketError.load(), self._transportError.load());
        }
    }
    else if (id == MQTT_EVENT_DELETED) ++self._errors;
    else if (id == MQTT_EVENT_DATA) {
        if (e->current_data_offset == 0) ++self._received;
        // Only complete, bounded messages are handled. Retained HA birth is safe; commands aren't.
        if (e->current_data_offset || e->data_len != e->total_data_len ||
            e->data_len <= 0 || e->data_len >= 32) return;
        String topic; topic.concat(e->topic, e->topic_len);
        String payload; payload.concat(e->data, e->data_len);
        if (topic == "homeassistant/status") {
            if (payload == "online") self._announce = true;
            return;
        }
        if (topic != self._base + "/command" || e->retain) return;
        bool allowed = payload == "led_on" || payload == "led_off";
        for (auto action : actions) allowed = allowed || payload == action;
        if (!allowed) return;
        Command command = {};
        payload.toCharArray(command.text, sizeof(command.text));
        if (!self._commands || xQueueSend(self._commands, &command, 0) != pdTRUE) ++self._errors;
    }
}

bool MqttService::publish(const String& topic, const String& payload, bool retain) {
    if (!_connected || !_client) return false;
    if (esp_mqtt_client_enqueue(_client, topic.c_str(), payload.c_str(), payload.length(), 1, retain, true) < 0) {
        ++_errors; return false;
    }
    return true;
}

bool MqttService::discovery(size_t index) {
    JsonDocument doc;
    String key, component = "sensor";
    if (index < SENSOR_COUNT) {
        const auto& sensor = sensors[index]; key = sensor.key;
        doc["name"] = sensor.name; doc["unit_of_measurement"] = sensor.unit;
        doc["device_class"] = sensor.deviceClass; doc["state_class"] = "measurement";
        doc["state_topic"] = _base + "/state";
        doc["value_template"] = "{{ value_json." + key + " if value_json." + key + " is not none else 'None' }}";
        doc["expire_after"] = 120;
    } else if (index < SENSOR_COUNT + ACTION_COUNT) {
        size_t action = index - SENSOR_COUNT;
        key = "action_" + String(action); component = "button";
        doc["name"] = actionNames[action]; doc["command_topic"] = _base + "/command";
        doc["payload_press"] = actions[action];
    } else if (index == SENSOR_COUNT + ACTION_COUNT) {
        key = "led"; component = "switch"; doc["name"] = "LED";
        doc["command_topic"] = _base + "/command"; doc["state_topic"] = _base + "/state";
        doc["value_template"] = "{{ value_json.led }}";
        doc["payload_on"] = "led_on"; doc["payload_off"] = "led_off";
        doc["state_on"] = "ON"; doc["state_off"] = "OFF";
    } else if (index == SENSOR_COUNT + ACTION_COUNT + 1) {
        key = "sd_mounted"; component = "binary_sensor"; doc["name"] = "SD disponivel";
        doc["state_topic"] = _base + "/state";
        doc["value_template"] = "{{ value_json.sd_mounted }}";
        doc["payload_on"] = "ON"; doc["payload_off"] = "OFF";
    } else if (index == SENSOR_COUNT + ACTION_COUNT + 2) {
        key = "navigation_button"; component = "event"; doc["name"] = "Botao GPIO27";
        doc["state_topic"] = _base + "/event";
        doc["event_types"].to<JsonArray>().add("press");
    } else {
        key = "edge_alert"; component = "binary_sensor"; doc["name"] = "Alerta local";
        doc["device_class"] = "problem"; doc["state_topic"] = _base + "/state";
        doc["value_template"] = "{{ value_json.edge_alert }}";
        doc["payload_on"] = "ON"; doc["payload_off"] = "OFF";
    }
    doc["unique_id"] = _id + "_" + key;
    doc["availability_topic"] = _base + "/availability";
    doc["device"]["identifiers"].to<JsonArray>().add(_id);
    doc["device"]["name"] = "ESP32-P4 " + _id.substring(8);
    doc["device"]["manufacturer"] = "Waveshare";
    doc["device"]["model"] = "ESP32-P4"; doc["device"]["sw_version"] = FIRMWARE_VERSION;
    return publish("homeassistant/" + component + "/" + _id + "/" + key + "/config", jsonText(doc));
}

void MqttService::state(StorageManager& storage) {
    JsonDocument doc;
    float cpu = readCpuTemperatureCelsius();
    if (isfinite(cpu)) doc["cpu_temperature"] = cpu; else doc["cpu_temperature"] = nullptr;
    auto climate = weather.snapshot();
    if (climate.available) doc["temperature"] = climate.temperature; else doc["temperature"] = nullptr;
    if (climate.available && isfinite(climate.humidity)) doc["humidity"] = climate.humidity; else doc["humidity"] = nullptr;
    doc["uptime"] = esp_timer_get_time() / 1000000ULL;
    doc["heap_free"] = ESP.getFreeHeap();
    if (WiFi.status() == WL_CONNECTED) doc["wifi_rssi"] = WiFi.RSSI(); else doc["wifi_rssi"] = nullptr;
    const auto& sd = storage.telemetry();
    doc["sd_mounted"] = sd.mounted ? "ON" : "OFF";
    if (sd.mounted) { doc["sd_free"] = sd.total - sd.used; doc["sd_used"] = sd.used; }
    else { doc["sd_free"] = nullptr; doc["sd_used"] = nullptr; }
    doc["led"] = digitalRead(LED_PIN) ? "ON" : "OFF";
    doc["edge_alert"] = edge.activeCount() ? "ON" : "OFF";
    doc["edge_enabled"] = edge.enabled(); doc["edge_active_count"] = edge.activeCount();
    if (publish(_base + "/state", jsonText(doc))) _publishAt = millis() + 30000;
    else _publishAt = millis() + 5000;
}

void MqttService::handle(StorageManager& storage, bool busy, void (*command)(String)) {
    if (busy) { if (_commands) xQueueReset(_commands); return; }
    if (_pending && int32_t(millis() - _applyAt) >= 0) { stop(); _pending = false; }
    if (_pending) return;
    bool network = WiFi.status() == WL_CONNECTED || (ETH.linkUp() && ETH.localIP() != IPAddress(0,0,0,0));
    if (!_client && _config.enabled && network && int32_t(millis() - _applyAt) >= 0) {
        start(); _applyAt = millis() + 10000;
    }
    if (!_connected) { if (_commands) xQueueReset(_commands); return; }
    if (_announce.load() && int32_t(millis() - _announceAt) >= 0 && _announce.exchange(false)) {
        if (esp_mqtt_client_subscribe(_client, (_base + "/command").c_str(), 0) < 0 ||
            esp_mqtt_client_subscribe(_client, "homeassistant/status", 0) < 0) { ++_errors; _announce = true; }
        _announceAt = millis() + 5000;
        _discoveryIndex = 0; _discovering = true; _discoveryAt = millis();
        _publishAt = millis();
    }
    // Spread discovery across cycles and bound ESP-MQTT's outbox.
    if (_discovering && int32_t(millis() - _discoveryAt) >= 0) {
        bool ok = discovery(_discoveryIndex);
        _discoveryAt = millis() + (ok ? 50 : 1000);
        if (ok && ++_discoveryIndex == SENSOR_COUNT + ACTION_COUNT + 4) {
            _discovering = false;
            if (!publish(_base + "/availability", "online")) _announce = true;
        }
    }
    Command next;
    if (_commands && xQueueReceive(_commands, &next, 0) == pdTRUE && command) {
        command(String(next.text)); _publishAt = millis();
    }
    if (int32_t(millis() - _publishAt) >= 0) state(storage);
}

void MqttService::buttonPressed() { publish(_base + "/event", "{\"event_type\":\"press\"}", false); }

void MqttService::edgeEvent(const char* rule, bool active, uint64_t sequence, uint64_t uptime) {
    JsonDocument doc; doc["rule"] = rule; doc["active"] = active;
    doc["sequence"] = sequence; doc["uptime"] = uptime;
    publish(_base + "/edge/event", jsonText(doc), false);
    _publishAt = millis();
}

String MqttService::configuration() {
    JsonDocument doc;
    doc["enabled"] = _config.enabled; doc["host"] = _config.host;
    doc["port"] = _config.port; doc["user"] = _config.user;
    doc["password_set"] = !_config.password.isEmpty(); doc["connected"] = connected();
    doc["base_topic"] = _base; doc["sent"] = sent(); doc["received"] = received(); doc["errors"] = errors();
    doc["disconnects"] = _disconnects.load(); doc["last_error_type"] = _errorType.load();
    doc["broker_return_code"] = _brokerCode.load(); doc["socket_errno"] = _socketError.load();
    doc["transport_error"] = _transportError.load();
    doc["heap_free"] = ESP.getFreeHeap();
    doc["network_up"] = WiFi.status() == WL_CONNECTED || (ETH.linkUp() && ETH.localIP() != IPAddress(0,0,0,0));
    return jsonText(doc);
}
String MqttService::status() { return configuration() + "\n"; }

void MqttService::save(WebServer& server) {
    if (_pending) { server.send(409, "text/plain", "Aguarde a aplicacao da configuracao anterior"); return; }
    if (server.header("X-ESP32-Command") != "1" || !server.hasArg("host") ||
        !server.hasArg("port") || !server.hasArg("user") || !server.hasArg("enabled")) {
        server.send(400, "text/plain", "Formulario incompleto"); return;
    }
    Config next = _config;
    String enabled = server.arg("enabled"), port = server.arg("port");
    next.enabled = enabled == "1"; next.host = server.arg("host"); next.host.trim();
    next.user = server.arg("user");
    bool digits = !port.isEmpty() && port.length() <= 5;
    for (size_t i = 0; i < port.length(); ++i) digits = digits && isDigit(port[i]);
    long number = port.toInt();
    if ((enabled != "0" && enabled != "1") || !digits || number < 1 || number > 65535 ||
        (next.enabled && !validHost(next.host)) || (!next.host.isEmpty() && !validHost(next.host)) ||
        next.user.length() > 128 || server.arg("password").length() > 128) {
        server.send(400, "text/plain", "Confira servidor, porta e credenciais (maximo 128 caracteres)."); return;
    }
    next.port = number;
    if (server.arg("clear_password") == "1") next.password = "";
    else if (!server.arg("password").isEmpty()) next.password = server.arg("password");
    JsonDocument doc;
    doc["enabled"] = next.enabled; doc["host"] = next.host; doc["port"] = next.port;
    doc["user"] = next.user; doc["password"] = next.password;
    String encoded = jsonText(doc);
    Preferences prefs;
    if (!prefs.begin("mqtt", false)) { server.send(503, "text/plain", "NVS indisponivel"); return; }
    String previous = prefs.getString("config", "");
    bool ok = prefs.putString("config", encoded) == encoded.length() && prefs.getString("config", "") == encoded;
    if (!ok) { if (previous.isEmpty()) prefs.remove("config"); else prefs.putString("config", previous); }
    prefs.end();
    if (!ok) { server.send(503, "text/plain", "Falha ao salvar MQTT; confira a configuracao antes de reiniciar."); return; }
    // Client copies credentials on init; stop the old connection before applying new credentials.
    _config = next; _pending = true; _applyAt = millis() + 1000;
    server.send(200, "application/json", configuration());
}

void MqttService::routes(WebServer& server, bool (*busy)()) {
    server.on("/mqtt", HTTP_GET, [this, &server]() { server.sendHeader("Cache-Control", "no-store"); server.send(200, "text/html; charset=utf-8", page()); });
    server.on("/api/mqtt/config", HTTP_GET, [this, &server]() { server.sendHeader("Cache-Control", "no-store"); server.send(200, "application/json", configuration()); });
    server.on("/api/mqtt/config", HTTP_POST, [this, &server, busy]() {
        server.sendHeader("Cache-Control", "no-store");
        if (busy && busy()) { server.send(409, "text/plain", "Aguarde OTA ou reinicio"); return; }
        save(server);
    });
}

String MqttService::page() {
    String html = portalStart("MQTT", false, false, false, true);
    html += F("<header class='page-header'><h1>MQTT e Home Assistant</h1></header><section class='card'>");
    html += F("<p>Configure o broker MQTT do Home Assistant. Descoberta automática em homeassistant. Publicação a cada 30 segundos, via Wi-Fi ou Ethernet. Conexão TCP sem TLS, para sua rede local.</p>");
    html += FPSTR(MQTT_FORM);
    html += F("</section>"); return html + portalEnd();
}
