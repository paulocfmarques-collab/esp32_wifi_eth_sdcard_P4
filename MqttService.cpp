#include "MqttService.h"
#include "PortalUi.h"
#include "DeviceConfig.h"
#include "FirmwareVersion.h"
#include "WeatherService.h"
#include "CpuTemperature.h"
#include "EdgeComputing.h"
#include "SystemLog.h"
#include "RadarServo.h"
#include "UltrasonicService.h"
#include <WiFi.h>
#include <ETH.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <ctype.h>
#include <NetworkClient.h>

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
const char* actions[] = {"tela:next", "tela:auto", "protetor:on", "protetor:off", "audio_test", "camera_oled",
    "servo:on", "servo:off", "servo:angulo:90", "servo:status",
    "ultrasonic:on", "ultrasonic:off", "ultrasonic:status", nullptr, nullptr, "tela:7"};
const char* actionNames[] = {"Proxima tela", "Retomar tela", "Carinhas ligar", "Carinhas desligar", "Testar som", "Camera no OLED",
    "Iniciar radar", "Parar servo", "Centralizar servo", "Atualizar estado do servo",
    "Ligar ultrassom", "Desligar ultrassom", "Atualizar ultrassom", nullptr, nullptr, "Sonar no OLED"};
constexpr size_t SENSOR_COUNT = sizeof(sensors) / sizeof(sensors[0]);
constexpr size_t ACTION_COUNT = sizeof(actions) / sizeof(actions[0]);
static_assert(ACTION_COUNT == sizeof(actionNames) / sizeof(actionNames[0]), "MQTT action names must match commands");
bool servoAngleCommand(const String& payload) {
    if (!payload.startsWith("servo:angulo:")) return false;
    const String number = payload.substring(13);
    if (number.isEmpty() || number.length() > 3) return false;
    for (char c : number) if (c < '0' || c > '9') return false;
    const int angle = number.toInt();
    return angle >= SERVO_MIN_ANGLE && angle <= SERVO_MAX_ANGLE;
}
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
    if (!validHost(_config.host) || !_config.port || _config.user.length() > 128 || _config.password.length() > 128)
        _config.enabled = false;
    _connections = xQueueCreate(1, sizeof(Connection));
    _publications = xQueueCreate(7, sizeof(Publication)); // About 15.5 KiB, bounded.
    if (!_commands || !_connections || !_publications ||
        xTaskCreatePinnedToCore(run, "mqtt_io", 8192, this, 1, &_worker, 0) != pdPASS) {
        _worker = nullptr; ++_errors; systemLog.record("MQTT", "falha ao criar tarefa/filas"); return;
    }
    start();
}

void MqttService::start() {
    if (!_connections || !_worker) return;
    Connection request;
    request.enabled = _config.enabled;
    _config.host.toCharArray(request.host, sizeof(request.host));
    _config.user.toCharArray(request.user, sizeof(request.user));
    _config.password.toCharArray(request.password, sizeof(request.password));
    request.port = _config.port;
    request.generation = ++_generation;
    _connected = false; _announce = false; _discovering = false;
    if (_commands) xQueueReset(_commands);
    xQueueOverwrite(_connections, &request);
}

void MqttService::run(void* context) {
    auto& self = *static_cast<MqttService*>(context);
    NetworkClient transport;
    transport.setConnectionTimeout(5000);
    MqttClient client(transport);
    client.setConnectionTimeout(5000);
    client.setKeepAliveInterval(30000);
    client.setId(self._id);
    client.onMessage(message);
    self._client = &client;
    Connection current;
    uint32_t retryAt = 0;
    Publication publication;
    for (;;) {
        Connection request;
        if (xQueueReceive(self._connections, &request, 0) == pdTRUE) {
            if (client.connected()) {
                client.beginMessage(self._base + "/availability", (unsigned long)7, true, 1);
                client.print("offline"); client.endMessage();
            }
            client.stop();
            self._connected = false;
            xQueueReset(self._publications);
            current = request; self._workerGeneration = current.generation;
            client.setUsernamePassword(current.user, current.password);
            client.beginWill(self._base + "/availability", (unsigned short)7, true, 1);
            client.print("offline"); client.endWill();
            retryAt = millis();
        }
        if (!current.enabled || current.generation != self._generation.load()) {
            vTaskDelay(pdMS_TO_TICKS(20)); continue;
        }
        const bool network = WiFi.status() == WL_CONNECTED ||
            (ETH.linkUp() && ETH.localIP() != IPAddress(0,0,0,0));
        if (!network || !client.connected()) {
            if (self._connected.exchange(false)) {
                ++self._disconnects;
                systemLog.record("MQTT", "desconectado; reconexao em 10s");
                client.stop(); retryAt = millis() + 10000;
            }
            if (network && int32_t(millis() - retryAt) >= 0) {
                retryAt = millis() + 10000;
                if (!client.connect(current.host, current.port)) {
                    const int error = client.connectError();
                    self._errorType = error > 0 ? 2 : 1;
                    self._brokerCode = error > 0 ? error : 0;
                    self._transportError = error;
                    ++self._errors;
                    systemLog.record("MQTT", "conexao falhou codigo=%d", error);
                    client.stop();
                } else if (current.generation != self._generation.load()) {
                    client.stop();
                } else if (!client.subscribe(self._base + "/command", 0) ||
                           !client.subscribe("homeassistant/status", 0)) {
                    ++self._errors; client.stop();
                } else {
                    self._errorType = 0; self._brokerCode = 0;
                    self._transportError = 0; self._socketError = 0;
                    self._connected = true; self._announce = true;
                    systemLog.record("MQTT", "conectado (ArduinoMqttClient)");
                }
            }
            vTaskDelay(pdMS_TO_TICKS(20)); continue;
        }
        client.poll();
        if (current.generation == self._generation.load() && client.connected() &&
            xQueueReceive(self._publications, &publication, 0) == pdTRUE) {
            if (publication.generation == current.generation) {
                const size_t size = strlen(publication.payload);
                bool ok = client.beginMessage(publication.topic, (unsigned long)size, publication.retain, 1);
                if (ok) ok = client.write(reinterpret_cast<const uint8_t*>(publication.payload), size) == size;
                if (ok) ok = client.endMessage() == 1;
                if (ok) ++self._sent;
                else { ++self._errors; client.stop(); }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void MqttService::message(int size) {
    auto& self = mqtt;
    auto& client = *self._client;
    ++self._received;
    if (size <= 0 || size >= 32 ||
        self._workerGeneration != self._generation.load()) return;
    const String topic = client.messageTopic();
    const bool retained = client.messageRetain() == 1;
    char buffer[32]{};
    if (client.read(reinterpret_cast<uint8_t*>(buffer), size) != size) { ++self._errors; return; }
    if (memchr(buffer, 0, size)) return; // Never execute a prefix of a binary command.
    const String payload(buffer);
    if (topic == "homeassistant/status") {
        if (payload == "online") self._announce = true;
        return;
    }
    if (topic != self._base + "/command" || retained) return;
    bool allowed = payload == "led_on" || payload == "led_off" || servoAngleCommand(payload);
    for (auto action : actions) if (action) allowed = allowed || payload == action;
    if (!allowed) return;
    Command command = {};
    payload.toCharArray(command.text, sizeof(command.text));
    if (xQueueSend(self._commands, &command, 0) != pdTRUE) ++self._errors;
}

bool MqttService::publish(const String& topic, const String& payload, bool retain) {
    if (!_connected || !_publications) return false;
    Publication publication;
    if (topic.length() >= sizeof(publication.topic) || payload.length() >= sizeof(publication.payload)) {
        ++_errors; return false;
    }
    topic.toCharArray(publication.topic, sizeof(publication.topic));
    payload.toCharArray(publication.payload, sizeof(publication.payload));
    publication.retain = retain; publication.generation = _generation.load();
    if (xQueueSend(_publications, &publication, 0) != pdTRUE) { ++_errors; return false; }
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
        // Keep the existing action IDs, clearing only the retired tracking buttons.
        if (!actions[action]) return publish("homeassistant/button/" + _id + "/" + key + "/config", "");
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
    } else if (index == SENSOR_COUNT + ACTION_COUNT + 3) {
        key = "edge_alert"; component = "binary_sensor"; doc["name"] = "Alerta local";
        doc["device_class"] = "problem"; doc["state_topic"] = _base + "/state";
        doc["value_template"] = "{{ value_json.edge_alert }}";
        doc["payload_on"] = "ON"; doc["payload_off"] = "OFF";
    } else if (index == SENSOR_COUNT + ACTION_COUNT + 4) {
        key = "servo_angle"; component = "number"; doc["name"] = "Angulo do servo";
        doc["command_topic"] = _base + "/command";
        doc["command_template"] = "servo:angulo:{{ value | int }}";
        doc["state_topic"] = _base + "/servo/state";
        doc["value_template"] = "{{ value_json.angle }}";
        doc["min"] = SERVO_MIN_ANGLE; doc["max"] = SERVO_MAX_ANGLE; doc["step"] = 1;
        doc["mode"] = "slider"; doc["unit_of_measurement"] = "°";
        doc["retain"] = false; doc["icon"] = "mdi:rotate-right";
    } else if (index == SENSOR_COUNT + ACTION_COUNT + 5) {
        key = "servo_status"; doc["name"] = "Estado do servo";
        doc["state_topic"] = _base + "/servo/state";
        doc["value_template"] = "{{ value_json.mode }}";
        doc["json_attributes_topic"] = _base + "/servo/state";
        doc["icon"] = "mdi:radar";
    } else if (index == SENSOR_COUNT + ACTION_COUNT + 6 || index == SENSOR_COUNT + ACTION_COUNT + 7) {
        const bool distance = index == SENSOR_COUNT + ACTION_COUNT + 6;
        key = distance ? "ultrasonic_distance" : "ultrasonic_angle";
        doc["name"] = distance ? "Distancia ultrassonica" : "Angulo da leitura ultrassonica";
        doc["unit_of_measurement"] = distance ? "cm" : "°";
        if (distance) doc["device_class"] = "distance";
        doc["state_class"] = "measurement";
        doc["state_topic"] = _base + "/servo/state";
        const String field = distance ? "distance_cm" : "distance_angle";
        doc["value_template"] = "{{ value_json." + field + " if value_json." + field + " is not none else 'None' }}";
        doc["expire_after"] = 10;
        doc["json_attributes_topic"] = _base + "/servo/state";
    } else if (index == SENSOR_COUNT + ACTION_COUNT + 8) {
        return publish("homeassistant/binary_sensor/" + _id + "/radar_target/config", "");
    } else {
        return false;
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
    if (_pending && int32_t(millis() - _applyAt) >= 0) { start(); _pending = false; }
    if (_pending) return;
    if (!_connected) { if (_commands) xQueueReset(_commands); return; }
    if (_announce.load() && int32_t(millis() - _announceAt) >= 0 && _announce.exchange(false)) {
        _announceAt = millis() + 5000;
        _discoveryIndex = 0; _discovering = true; _discoveryAt = millis();
        _publishAt = millis(); _servoPublishAt = millis();
    }
    // Spread discovery across cycles; publication queue applies bounded backpressure.
    if (_discovering && int32_t(millis() - _discoveryAt) >= 0) {
        bool ok = discovery(_discoveryIndex);
        _discoveryAt = millis() + (ok ? 50 : 1000);
        if (ok && ++_discoveryIndex == SENSOR_COUNT + ACTION_COUNT + 9) {
            _discovering = false;
            if (!publish(_base + "/availability", "online")) _announce = true;
        }
    }
    Command next;
    if (_commands && xQueueReceive(_commands, &next, 0) == pdTRUE && command) {
        command(String(next.text)); _publishAt = millis(); _servoPublishAt = millis();
    }
    if (int32_t(millis() - _publishAt) >= 0) state(storage);
    if (int32_t(millis() - _servoPublishAt) >= 0) servoState();
}

void MqttService::servoState() {
    JsonDocument doc;
    doc["mode"] = radarServo.mode();
    doc["angle"] = radarServo.angle(); // Commanded PWM angle, not a physical position sensor.
    doc["target"] = radarServo.target();
    doc["min"] = SERVO_MIN_ANGLE; doc["max"] = SERVO_MAX_ANGLE;
    doc["tracking_enabled"] = radarServo.trackingEnabled();
    doc["target_locked"] = radarServo.targetLocked();
    if (radarServo.targetLocked()) {
        doc["target_angle"] = radarServo.trackedAngle();
        doc["target_distance_cm"] = radarServo.trackedDistance();
    } else { doc["target_angle"] = nullptr; doc["target_distance_cm"] = nullptr; }
    doc["search_step_degrees"] = SERVO_SWEEP_STEP_DEGREES;
    doc["sweep_interval_ms"] = RADAR_SWEEP_INTERVAL_MS;
    const auto sonar = ultrasonic.snapshot();
    doc["ultrasonic_available"] = sonar.available;
    if (sonar.available) doc["distance_cm"] = sonar.distanceCm; else doc["distance_cm"] = nullptr;
    if (sonar.angleKnown) doc["distance_angle"] = sonar.angle; else doc["distance_angle"] = nullptr;
    doc["ultrasonic_age_ms"] = sonar.ageMs;
    doc["ultrasonic_samples"] = sonar.samples; doc["ultrasonic_errors"] = sonar.errors;
    _servoPublishAt = millis() + (publish(_base + "/servo/state", jsonText(doc)) ? 1000 : 5000);
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
    doc["backend"] = "ArduinoMqttClient 0.1.8";
    doc["worker_ready"] = _worker != nullptr;
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
    // The worker receives a bounded configuration snapshot; no network calls on the application task.
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
