#include "NetworkController.h"

NetworkController* NetworkController::_instance = nullptr;
bool NetworkController::ethConectado = false;

NetworkController::NetworkController(OledDisplay& oledRef, StorageManager& storageRef)
  : _server(80), _oled(oledRef), _storage(storageRef), _dadosProntosParaSalvar(false), _rotasOk(false) {
  _instance = this;
}

void NetworkController::inicializar()
 {
  WiFi.onEvent(onNetworkEvent);
}

void NetworkController::onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (!_instance) return;
  switch (event) 
  {
    case ARDUINO_EVENT_ETH_START: _instance->_oled.adicionarLinha("ETH Inicializada"); ETH.setHostname("esp32-p4-node"); break;
    case ARDUINO_EVENT_ETH_CONNECTED: _instance->_oled.adicionarLinha("Cabo ETH Conectado"); break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      _instance->_oled.adicionarLinha("IP ETH Obtido:");
      _instance->_oled.adicionarLinha(ETH.localIP().toString());
      ethConectado = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED: _instance->_oled.adicionarLinha("ETH Desconectada"); ethConectado = false; break;
    default: break;
  }
}

static String esc(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

static const char CSS_P4[] PROGMEM =
  "<style>body{font-family:Arial;margin:20px;background:#f4f4f9;text-align:center;}"
  ".box{background:white;max-width:420px;margin:auto;padding:20px;border-radius:8px;box-shadow:0 4px 8px rgba(0,0,0,0.1);text-align:left;}"
  "input{width:100%;box-sizing:border-box;margin:3px 0;padding:6px;}"
  "td{padding:3px 6px;}a{color:#06c;}</style>";

void NetworkController::carregarRedes(String ssids[NUM_REDES], String senhas[NUM_REDES], int& idx) {
  _prefs.begin("wifi", false);
  String antigo = _prefs.getString("ssid", "");
  if (antigo != "" && _prefs.getString("s0", "") == "") {
    _prefs.putString("s0", antigo);
    _prefs.putString("p0", _prefs.getString("senha", ""));
  }
  if (antigo != "") { _prefs.remove("ssid"); _prefs.remove("senha"); }
  for (int i = 0; i < NUM_REDES; i++) {
    ssids[i]  = _prefs.getString(("s" + String(i)).c_str(), "");
    senhas[i] = _prefs.getString(("p" + String(i)).c_str(), "");
  }
  idx = _prefs.getInt("idx", -1);
  _prefs.end();
}

bool NetworkController::tentarRede(const String& ssid, const String& senha) {
  _oled.adicionarLinha("Conectando a:");
  _oled.adicionarLinha(ssid);
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), senha.c_str());

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 24) {
    delay(500);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    tentativas++;
  }
  digitalWrite(LED_PIN, LOW);
  return WiFi.status() == WL_CONNECTED;
}

bool NetworkController::conectarWifi() {
  String ssids[NUM_REDES], senhas[NUM_REDES];
  int idx;
  carregarRedes(ssids, senhas, idx);

  bool algumaSalva = false;
  for (int i = 0; i < NUM_REDES; i++) if (ssids[i] != "") algumaSalva = true;
  if (!algumaSalva) return false;

  WiFi.mode(WIFI_STA);
  _oled.adicionarLinha("Buscando redes...");
  int n = WiFi.scanNetworks();

  for (int k = 1; k <= NUM_REDES; k++) {
    int i = ((idx + k) % NUM_REDES + NUM_REDES) % NUM_REDES;
    if (ssids[i] == "") continue;

    bool visivel = false;
    for (int j = 0; j < n; j++) {
      if (WiFi.SSID(j) == ssids[i]) { visivel = true; break; }
    }
    if (!visivel) continue;

    if (tentarRede(ssids[i], senhas[i])) {
      _prefs.begin("wifi", false);
      _prefs.putInt("idx", i);
      _prefs.end();
      WiFi.scanDelete();
      _udp.begin(UDP_PORTA);
      configurarRotas();
      return true;
    }
  }
  WiFi.scanDelete();
  return false;
}

void NetworkController::configurarRotas() {
  if (_rotasOk) return;
  _server.on("/", HTTP_GET, handleRootCallback);
  _server.on("/info", HTTP_GET, handleInfoCallback);
  _server.on("/wifi", HTTP_GET, handleWifiCallback);
  _server.on("/salvar", HTTP_POST, handleSaveCallback);
  _server.begin();
  _rotasOk = true;
}

void NetworkController::iniciarPortal() 
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_P4_CONFIG");
  _oled.adicionarLinha("Portal Ativo!");
  _oled.adicionarLinha("IP: 192.168.4.1");
  configurarRotas();
}

void NetworkController::handleWifi() {
  String ssids[NUM_REDES], senhas[NUM_REDES];
  int idx;
  carregarRedes(ssids, senhas, idx);

  String h = F("<!DOCTYPE html><html lang='pt-BR'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>WiFi - ESP32-P4</title>");
  h += FPSTR(CSS_P4);
  h += F("</head><body><div class='box'><h2>Configuração WiFi - P4</h2><form action='/salvar' method='POST'>");
  for (int i = 0; i < NUM_REDES; i++) {
    h += "<b>Rede " + String(i + 1) + "</b><input type='text' name='ssid" + String(i) + "' placeholder='SSID' value='" + esc(ssids[i]) + "'>";
    h += "<input type='password' name='senha" + String(i) + "' placeholder='" + String(ssids[i] != "" ? "(manter senha atual)" : "Senha") + "'>";
  }
  h += F("<br><input type='submit' value='Salvar e Reiniciar'></form><p><a href='/info'>Informações</a></p></div></body></html>");
  _server.send(200, "text/html", h);
}

void NetworkController::handleInfo() {
  String ssids[NUM_REDES], senhas[NUM_REDES];
  int idx;
  carregarRedes(ssids, senhas, idx);

  uint32_t seg = millis() / 1000;
  String h = F("<!DOCTYPE html><html lang='pt-BR'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><meta http-equiv='refresh' content='10'><title>Info - ESP32-P4</title>");
  h += FPSTR(CSS_P4);
  h += F("</head><body><div class='box'><h2>ESP32-P4 - Informações</h2><table>");
  h += "<tr><td>Chip</td><td>" + String(ESP.getChipModel()) + " rev " + String(ESP.getChipRevision()) + "</td></tr>";
  h += "<tr><td>SSID</td><td>" + esc(WiFi.SSID()) + "</td></tr>";
  h += "<tr><td>IP Wi-Fi</td><td>" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("-")) + "</td></tr>";
  h += "<tr><td>IP ETH</td><td>" + (ethConectado ? ETH.localIP().toString() : String("-")) + "</td></tr>";
  h += "<tr><td>RSSI</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  h += "<tr><td>MAC</td><td>" + WiFi.macAddress() + "</td></tr>";
  h += "<tr><td>Heap livre</td><td>" + String(ESP.getFreeHeap() / 1024) + " KB</td></tr>";
  h += "<tr><td>SD Card</td><td>" + String(_storage.isAtivo() ? "OK" : "FALHA") + "</td></tr>";
  h += "<tr><td>Uptime</td><td>" + String(seg / 3600) + "h " + String((seg / 60) % 60) + "m " + String(seg % 60) + "s</td></tr>";
  h += F("</table><h3>Redes salvas</h3><ol>");
  for (int i = 0; i < NUM_REDES; i++) {
    if (ssids[i] == "") continue;
    h += "<li>" + esc(ssids[i]) + (i == idx ? " (última usada)" : "") + "</li>";
  }
  h += F("</ol><p><a href='/wifi'>Configurar redes</a></p></div></body></html>");
  _server.send(200, "text/html", h);
}

void NetworkController::handleRoot() {
  if (WiFi.getMode() == WIFI_AP) handleWifi();
  else handleInfo();
}

void NetworkController::handleSave() {
  String ssids[NUM_REDES], senhas[NUM_REDES];
  int idx;
  carregarRedes(ssids, senhas, idx);

  for (int i = 0; i < NUM_REDES; i++) {
    String novo = _server.arg("ssid" + String(i));
    String senha = _server.arg("senha" + String(i));
    novo.trim();
    if (novo == "") { _tmpS[i] = ""; _tmpP[i] = ""; continue; }
    _tmpS[i] = novo;
    _tmpP[i] = (senha == "" && novo == ssids[i]) ? senhas[i] : senha;
  }
  _server.send(200, "text/html", "<meta charset='UTF-8'><h2>Configuração salva! Reiniciando...</h2>");
  _dadosProntosParaSalvar = true;
}

void NetworkController::processarPortalERedes() 
{
  _server.handleClient();
  if (_dadosProntosParaSalvar) {
    _dadosProntosParaSalvar = false;
    _prefs.begin("wifi", false);
    for (int i = 0; i < NUM_REDES; i++) {
      _prefs.putString(("s" + String(i)).c_str(), _tmpS[i]);
      _prefs.putString(("p" + String(i)).c_str(), _tmpP[i]);
    }
    _prefs.putInt("idx", -1);
    _prefs.end();
    delay(1000);
    ESP.restart();
  }
}

void NetworkController::configurarPrioridadeDeRede() 
{
  esp_netif_t* netif_wifi = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_t* netif_eth  = esp_netif_get_handle_from_ifkey("ETH_DEF");
  if (netif_wifi != NULL && netif_eth != NULL) {
    esp_netif_set_route_prio(netif_wifi, 50);
    esp_netif_set_route_prio(netif_eth, 10);
  }
}

void NetworkController::responderUDP(const String& msg) 
{
  if (_udp.remoteIP()) {
    _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
    _udp.print(msg);
    _udp.endPacket();
  }
}

void NetworkController::zerarConfiguracoes() 
{
  _oled.adicionarLinha("Limpando Memoria...");
  _prefs.begin("wifi", false); _prefs.clear(); _prefs.end();
  responderUDP("WiFi zerado. Reiniciando...\n");
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_PIN, HIGH); delay(100);
    digitalWrite(LED_PIN, LOW); delay(100);
  }
  ESP.restart();
}

void NetworkController::ativarEthernetFisica() 
{
  _oled.adicionarLinha("Iniciando ETH...");
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
  ETH.setDefault();
}
