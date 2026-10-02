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
    network.ativarEthernetFisica(); 
    network.configurarPrioridadeDeRede();
    
    localTime.imprimirDataHora();
  } else {
    network.iniciarPortal();
  }
}

void loop() {
  network.processarPortalERedes();

  if (WiFi.status() == WL_CONNECTED) {
    oled.atualizarRelogioSuperior(localTime);
  }

  WiFiUDP& udp = network.getUdpDriver();
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char packetBuffer[255]; 
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
      cmdHandler.executar(String(packetBuffer));
    }
  }

  if (digitalRead(BOTAO_RESET) == LOW) {
    delay(50); 
    if (digitalRead(BOTAO_RESET) == LOW) {
      network.zerarConfiguracoes();
    }
  }
}
