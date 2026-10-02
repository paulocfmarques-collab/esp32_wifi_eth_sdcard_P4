#include "NetworkController.h"

const char htmlConfig[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR"><head><meta charset="UTF-8"><title>Configuração WiFi - ESP32-P4</title>
<style>body{font-family:Arial;margin:40px;background:#f4f4f9;text-align:center;}
.box{background:white;max-width:300px;margin:auto;padding:20px;border-radius:8px;box-shadow:0 4px 8px rgba(0,0,0,0.1);}</style></head>
<body><div class="box"><h2>Configuração WiFi - P4</h2><form action="/salvar" method="POST">
<input type="text" name="ssid" placeholder="SSID" required><br><input type="password" name="senha" placeholder="Senha"><br>
<input type="submit" value="Salvar"></form></div></body></html>
)rawliteral";

NetworkController* NetworkController::_instance = nullptr;
bool NetworkController::ethConectado = false;

NetworkController::NetworkController(OledDisplay& oledRef, StorageManager& storageRef)
  : _server(80), _oled(oledRef), _storage(storageRef), _dadosProntosParaSalvar(false) {
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

bool NetworkController::conectarWifi() {
  _prefs.begin("wifi", true);
  String ssid = _prefs.getString("ssid", "");
  String password = _prefs.getString("senha", "");
  _prefs.end();

  if (ssid == "") return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  _oled.adicionarLinha("Conectando a:");
  _oled.adicionarLinha(ssid);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) 
  {
    delay(500);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); 
    tentativas++;
  }
  digitalWrite(LED_PIN, LOW);
  
  if (WiFi.status() == WL_CONNECTED) 
  {
    _udp.begin(UDP_PORTA);
    return true;
  }
  return false;
}

void NetworkController::iniciarPortal() 
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_P4_CONFIG");
  _oled.adicionarLinha("Portal Ativo!");
  _oled.adicionarLinha("IP: 192.168.4.1");

  _server.on("/", HTTP_GET, handleRootCallback);
  _server.on("/salvar", HTTP_POST, handleSaveCallback);
  _server.begin();
}

void NetworkController::handleRoot() { _server.send(200, "text/html", htmlConfig); }
void NetworkController::handleSave() {
  _tempSSID = _server.arg("ssid"); _tempSenha = _server.arg("senha");
  _server.send(200, "text/html", "<h2>Configuracao salva! Reiniciando...</h2>");
  _dadosProntosParaSalvar = true;
}

void NetworkController::processarPortalERedes() 
{
  if (WiFi.getMode() == WIFI_AP) {
    _server.handleClient();
    if (_dadosProntosParaSalvar) {
      _dadosProntosParaSalvar = false;
      _prefs.begin("wifi", false);
      _prefs.putString("ssid", _tempSSID); _prefs.putString("senha", _tempSenha);
      _prefs.end();
      delay(1000);
      ESP.restart();
    }
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
