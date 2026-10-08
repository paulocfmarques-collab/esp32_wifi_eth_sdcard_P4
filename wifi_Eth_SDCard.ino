#include "DeviceConfig.h"
#include "OledDisplay.h"
#include "StorageManager.h"
#include "NetworkController.h"
#include "TimeManager.h"
#include "SlaveCommandHandler.h"

OledDisplay oled;
StorageManager storage;
NetworkController network(oled, storage);
TimeManager localTime(oled);
SlaveCommandHandler cmdHandler(oled, storage, network, localTime);

void setup() {
  Serial.begin(115200);
  delay(500); 

  pinMode(LED_PIN, OUTPUT);
  pinMode(BOTAO_RESET, INPUT_PULLUP);

  oled.inicializar();
  storage.inicializar();
  network.inicializar();
  network.ativarEthernetFisica();
  localTime.carregarEConfigurarHorario(); 

  if (network.conectarWifi()) {
    storage.gravarLog("Wi-Fi Conectado com Sucesso!");
    
    // ─── NOVO: ESCREVE O IP NA SERIAL E NO DISPLAY LOGO APÓS CONECTAR ───
    String ipLocal = WiFi.localIP().toString();
    Serial.println(F("\n-------------------------------------"));
    Serial.print(F("[WIFI] Conectado com sucesso! IP do Escravo: "));
    Serial.println(ipLocal);
    Serial.println(F("-------------------------------------"));

    // Força a exibição temporária do IP no display antes do menu principal
    oled.adicionarLinha("Conectado!");
    oled.adicionarLinha("IP: " + ipLocal);
    delay(3000); // Mantém a informação na tela por 3 segundos
    
    // Sincroniza o relógio NTP com a rede ativa
    localTime.inicializarETestarNTP();
    
    delay(1000);
    // Ethernet also starts when no Wi-Fi credentials are available. 
    network.configurarPrioridadeDeRede();
    
    localTime.imprimirDataHora();
  } else {
    network.iniciarPortal();
  }
}

void loop() {
  network.processarPortalERedes();
  cmdHandler.atualizar();

  oled.atualizarPainel(localTime, network, storage);

  WiFiUDP& udp = network.getUdpDriver();
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char packetBuffer[255]; 
    if (packetSize >= int(sizeof(packetBuffer))) {
      while (udp.available()) udp.read(packetBuffer, sizeof(packetBuffer));
      network.responderUDP("Erro: comando excede 254 bytes\n");
      return;
    }
    int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
    if (len > 0) {
      packetBuffer[len] = 0;
      cmdHandler.executar(String(packetBuffer));
    }
  }

  // Short press: next page. Hold five seconds: clear saved Wi-Fi settings.
  static bool resetArmed = false;
  static int readingBefore = HIGH, stable = HIGH;
  static uint32_t changedAt = 0, pressedAt = 0;
  int reading = digitalRead(BOTAO_RESET);
  if (reading != readingBefore) changedAt = millis();
  if (millis() - changedAt >= 50 && reading != stable) {
    stable = reading;
    if (stable == LOW) { pressedAt = millis(); resetArmed = false; }
    else if (resetArmed || millis() - pressedAt >= 5000) network.zerarConfiguracoes();
    else oled.avancarPagina();
  }
  readingBefore = reading;
  if (stable == LOW && !resetArmed && millis() - pressedAt >= 5000) {
    oled.adicionarLinha("Solte para limpar WiFi");
    resetArmed = true;
  }
}
