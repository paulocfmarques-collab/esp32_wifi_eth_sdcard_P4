#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include "esp_system.h"
#include "driver/temperature_sensor.h" 
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ETH.h>
#include "FS.h"
#include "SD_MMC.h"
#include "time.h"
#include <esp_sntp.h>
#include <esp_netif.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define MAX_LINHAS 8

// Definição dos pinos I2C para o OLED no ESP32-P4 Dev Kit
#define PIN_SDA 7
#define PIN_SCL 8

// Inicializa o objeto do display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define LED 1
#define BOTAO_RESET 2 

bool blinkAtivo = false;
bool estadoLed = false;
unsigned long ultimoToggle = 0;
unsigned long intervaloBlink = 500; 
String historicoLinhas[MAX_LINHAS];
int totalLinhas = 0;
bool oledInicializado = false;

// Variável para monitorar o status da conexão
bool eth_connected = false;
bool sdcardOk = false;

// Servidores NTP para buscar a hora
const char* ntpServer = "a.st1.ntp.br";

// Fuso horário (Exemplo: -10800 para o horário de Brasília - UTC-3)
// Compensação de horário de verão em segundos (0 se não houver)
const long  gmtOffset_sec = -10800; 
const int   daylightOffset_sec = 0;

WebServer server(80);
Preferences prefs;

// HTML da página de configuração
const char* htmlPage PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Configuração WiFi</title>
<style>
  body { font-family: Arial, sans-serif; margin: 40px; background-color: #f4f4f9; text-align: center; }
  .container { background: white; max-width: 300px; margin: auto; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
  input { width: 100%; padding: 8px; margin: 10px 0; box-sizing: border-box; }
  input[type="submit"] { background: #007bff; color: white; border: none; cursor: pointer; }
</style>
</head>
<body>
<div class="container">
  <h2>Configuração WiFi - ESP32-P4</h2>
  <form action="/salvar" method="POST">
    <label>SSID:</label>
    <input type="text" name="ssid" placeholder="Nome da rede" required>
    <label>Senha:</label>
    <input type="password" name="senha" placeholder="Senha da rede">
    <input type="submit" value="Salvar">
  </form>
</div>
</body>
</html>
)rawliteral";

WiFiUDP udp;
const int udpPort = 4210;

void setup() {
  Serial.begin(115200);
  delay(500); // Delay crucial para estabilização elétrica do chip C6

  Serial.println("----------------------------------------------------------------------------------------");

  pinMode(LED, OUTPUT);
  pinMode(BOTAO_RESET, INPUT_PULLUP); 

  // Inicializa o barramento I2C explicitando os pinos do P4
  Wire.begin(PIN_SDA, PIN_SCL); 
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    oledInicializado = true;
    display.clearDisplay();
    display.display();
    adicionarLinha("OLED Pronto!");
  } else {
    Serial.println("Falha ao encontrar o Display OLED nos pinos mapeados!");
  }

  sdcardOk = inicializa_sdcard();
  delay(1000);

  if (conectarWifi()) 
  {
    gravarLog("Wifi Conectado!");
    gravarLog(WiFi.localIP().toString());
    adicionarLinha("Wifi Conectado!");
    adicionarLinha(WiFi.localIP().toString());
    Serial.println("Wifi Conectado!");
    Serial.println(WiFi.localIP().toString());
    udp.begin(udpPort);
    // Inicializa e configura o tempo NTP
    gravarLog("[NTP] Configurando e iniciando sincronização...");
    inicializarETestarNTP();
    imprimirDataHora();
  } 
  else 
  {
    iniciarPortal();
  }

  WiFi.onEvent(onNetworkEvent);
  // Inicializa o hardware Ethernet com os parâmetros da placa
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
  ETH.setDefault();
  gravarLog("Prioridade de rota definida: Ethernet possui preferência sobre o Wi-Fi.");
  configurarPrioridadeDeRede();
  Serial.println("----------------------------------------------------------------------------------------");
}

void loop() {
  if (WiFi.getMode() == WIFI_AP) {
    server.handleClient();
  }

  int packetSize = udp.parsePacket();
  if (packetSize) {
    char packetBuffer[255];
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    String comando = String(packetBuffer);
    comando.trim();
    executa_comando(comando);
  }

  if (digitalRead(BOTAO_RESET) == LOW) {
    delay(50); 
    if (digitalRead(BOTAO_RESET) == LOW) {
      zerarConfiguracoes();
    }
  }

  if (blinkAtivo) {
    unsigned long atual = millis();
    if (atual - ultimoToggle >= intervaloBlink) {
      ultimoToggle = atual;
      estadoLed = !estadoLed;
      digitalWrite(LED, estadoLed);
    }
  }
}
