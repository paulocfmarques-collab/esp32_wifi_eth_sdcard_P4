#include "FirmwareUpdater.h"
#include "PortalUi.h"
#include "SystemLog.h"
#include <Update.h>
#include <esp_ota_ops.h>
#include <ctype.h>
#include <WiFi.h>
#include <ETH.h>

void FirmwareUpdater::begin(WebServer& server, OledDisplay& oled) {
  _server = &server; _oled = &oled;
  _password = "123456";
  _enabled = true;
  if (_enabled) {
    imprimirCredenciais();
    ArduinoOTA.setHostname("esp32-p4-node");
    ArduinoOTA.setPassword(_password.c_str());
    ArduinoOTA.setRebootOnSuccess(false);
    ArduinoOTA.setTimeout(10000);
    ArduinoOTA.onStart([this]() {
      systemLog.record("OTA", "inicio Arduino IDE");
      _busy = true;
      _lastProgress = -1;
      if (ArduinoOTA.getCommand() != U_FLASH) { Update.abort(); return; }
      _oled->adicionarLinha("OTA Arduino IDE");
    });
    ArduinoOTA.onProgress([this](unsigned int done, unsigned int total) {
      if (total) progresso(uint64_t(done) * 100 / total);
    });
    ArduinoOTA.onEnd([this]() {
      systemLog.record("OTA", "sucesso Arduino IDE; reinicio agendado");
      _oled->configurarProgresso(false, 0);
      _oled->adicionarLinha("OTA OK. Reiniciando");
      _restartAt = millis() + 1500;
    });
    ArduinoOTA.onError([this](ota_error_t error) {
      systemLog.record("OTA", "falha Arduino IDE codigo=%u", unsigned(error));
      _busy = false; _oled->configurarProgresso(false, 0);
      Serial.printf("[OTA] Falha Arduino IDE: %u\n", unsigned(error));
    });
  }
  server.on("/firmware", HTTP_GET, [this]() { page(); });
  server.on("/api/firmware", HTTP_POST, [this]() { complete(); }, [this]() { upload(); });
}

bool FirmwareUpdater::authenticate() {
  if (!_enabled) { _server->send(503, "text/plain; charset=utf-8", "OTA indisponivel: verifique a serial\n"); return false; }
  if (!_server->authenticate("admin", _password.c_str())) {
    _server->requestAuthentication(); return false;
  }
  return true;
}

void FirmwareUpdater::page() {
  if (!authenticate()) return;
  const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
  const esp_partition_t* running = esp_ota_get_running_partition();
  const bool available = target && running && target->address != running->address;
  String h = portalStart("Atualizar firmware", false);
  h += F("<div class='eyebrow'>Manutenção</div><h1>Atualizar firmware</h1><p class='muted'>Envie uma nova versão pela rede Wi-Fi ou Ethernet.</p><article class='card message'><h2>Arquivo do aplicativo (.bin)</h2><p>Use o arquivo exportado pelo Arduino IDE para a mesma placa e configuração de flash. Não use bootloader, partições ou imagem merged.</p>");
  if (!available) h += F("<div class='notice'>Partição OTA indisponível. Instale primeiro por USB com um esquema de partições que permita OTA.</div>");
  else {
    h += "<p class='hint'>Limite da partição: " + String(target->size) + " bytes. A versão atual permanece na partição ativa durante a transferência.</p>";
    h += FPSTR(PORTAL_FIRMWARE);
    h += "<script>window.firmwareLimit=" + String(target->size) + ";</script>";
  }
  h += F("<p class='hint section'>Arduino IDE: selecione a porta de rede esp32-p4-node e use a mesma senha mostrada na serial. A primeira instalação desta versão exige USB. Mantenha a alimentação até concluir o reinício.</p></article>");
  h += portalEnd();
  _server->sendHeader("Cache-Control", "no-store");
  _server->send(200, "text/html; charset=utf-8", h);
}

void FirmwareUpdater::fail(const String& error) {
  systemLog.record("OTA", "falha upload: %s", error.c_str());
  _error = error;
  if (_ownsUpload) { Update.abort(); _ownsUpload = false; }
  _busy = false; _oled->configurarProgresso(false, 0);
}

void FirmwareUpdater::upload() {
  HTTPUpload& file = _server->upload();
  if (file.status == UPLOAD_FILE_START) {
    _authorized = _enabled && _server->header("X-ESP32-Firmware") == "1" &&
      _server->authenticate("admin", _password.c_str());
    if (!_authorized) return;
    _error = ""; _received = 0; _expected = 0;
    if (_ownsUpload || busy() || Update.isRunning()) { _error = "Atualizacao/reinicio em andamento"; return; }
    if (file.name != "firmware" || !file.filename.endsWith(".bin")) { _error = "Envie somente um arquivo de aplicativo .bin"; return; }
    String size = _server->arg("size");
    if (size.isEmpty() || size.length() > 9) { _error = "Tamanho invalido"; return; }
    for (size_t i=0; i<size.length(); ++i) if (!isdigit((unsigned char)size[i])) { _error = "Tamanho invalido"; return; }
    _expected = size.toInt();
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!target || !running || target->address == running->address || !_expected || _expected > target->size) {
      _error = "Arquivo excede a particao ou particao OTA indisponivel"; return;
    }
    if (!Update.begin(_expected, U_FLASH)) { _error = Update.errorString(); return; }
    _busy = _ownsUpload = true; _lastChunkAt = millis();
    systemLog.record("OTA", "inicio navegador bytes=%lu", (unsigned long)_expected);
    _lastProgress = -1;
    _oled->adicionarLinha("OTA navegador");
  } else if (file.status == UPLOAD_FILE_WRITE && _authorized && _ownsUpload && _error.isEmpty()) {
    _lastChunkAt = millis();
    if (file.currentSize > _expected - _received) { fail("Arquivo maior que o tamanho informado"); return; }
    if (Update.write(file.buf, file.currentSize) != file.currentSize) { fail(Update.errorString()); return; }
    _received += file.currentSize;
    progresso(uint64_t(_received) * 100 / _expected);
  } else if (file.status == UPLOAD_FILE_END && _authorized && _ownsUpload) {
    // Finalize in complete(): all multipart fields/files must be consumed first.
    if (_received != _expected || file.totalSize != _expected) fail("Transferencia incompleta");
  } else if (file.status == UPLOAD_FILE_ABORTED && _authorized && _ownsUpload) fail("Transferencia interrompida");
}

void FirmwareUpdater::complete() {
  if (!authenticate()) return;
  _server->sendHeader("Cache-Control", "no-store");
  if (!_authorized || !_error.isEmpty() || !_ownsUpload) {
    if (_ownsUpload) fail(_error.isEmpty() ? String("Upload invalido") : _error);
    _server->send(400, "text/plain; charset=utf-8", _error.isEmpty() ? "Upload invalido" : _error); return;
  }
  if (!Update.end(false)) {
    fail(Update.errorString()); _server->send(400, "text/plain; charset=utf-8", _error); return;
  }
  _ownsUpload = false;
  systemLog.record("OTA", "sucesso navegador bytes=%lu; reinicio agendado", (unsigned long)_received);
  _oled->configurarProgresso(false, 0);
  _server->send(200, "text/plain; charset=utf-8", "Firmware verificado e ativado. Reiniciando em instantes. Reabra o painel apos o reinicio.");
  _restartAt = millis() + 1500;
}

void FirmwareUpdater::handle() {
  if (!_enabled) return;
  if (!_otaStarted) {
    ArduinoOTA.begin(); _otaStarted = true;
  }
  if (_ownsUpload && uint32_t(millis() - _lastChunkAt) > 15000) fail("Timeout do upload");
  if (_otaStarted && !_ownsUpload && !_restartAt) ArduinoOTA.handle();
  if (_restartAt && int32_t(millis() - _restartAt) >= 0) ESP.restart();
}

void FirmwareUpdater::imprimirCredenciais() const {
  if (_enabled) Serial.println("[OTA] Usuario: admin | Senha: " + _password);
  else Serial.println(F("[OTA] Indisponivel: senha nao configurada."));
}

void FirmwareUpdater::progresso(int percent) {
  if (percent == _lastProgress) return;
  _lastProgress = percent;
  _oled->configurarProgresso(true, percent);
}

String FirmwareUpdater::status() const {
  const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
  const esp_partition_t* running = esp_ota_get_running_partition();
  bool available = target && running && target->address != running->address;
  return "OTA: " + String(_enabled ? "habilitado" : "desabilitado") +
    "\nParticao OTA: " + String(available ? target->size : 0) + " bytes\nArduino IDE: " +
    String(_otaStarted ? "ativo" : "aguardando") + "\nHost: esp32-p4-node\nPorta OTA: 3232\nNavegador: /firmware\n";
}
