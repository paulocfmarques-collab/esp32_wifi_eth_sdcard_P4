#include "DeviceConfig.h"
#include "OledDisplay.h"
#include "StorageManager.h"
#include "NetworkController.h"
#include "TimeManager.h"
#include "SlaveCommandHandler.h"
#include "PageAudio.h"
#include "WeatherService.h"

OledDisplay oled;
StorageManager storage;
NetworkController network(oled, storage);
TimeManager localTime(oled);
SlaveCommandHandler cmdHandler(oled, storage, network, localTime);
static TaskHandle_t applicationTask = nullptr;
void executarCiclo();

static void applicationLoop(void*)
{
    Serial.println("[CORES] Web, UDP, OLED, SD e OTA no core " + String(xPortGetCoreID()) + ".");
    for (;;)
    {
        executarCiclo();
        vTaskDelay(1);
    }
}

void setup()
{
    Serial.begin(115200);
    cmdHandler.iniciarSerial();
    network.setCommandHandler(
        [](String command)
        { cmdHandler.executar(command, NetworkController::CommandChannel::HTTP); });
    delay(500);

    pinMode(LED_PIN, OUTPUT);
    pinMode(BOTAO_RESET, INPUT_PULLUP);

    oled.inicializar();
    pageAudio.begin();
    oled.aoTrocarPagina([]() { pageAudio.play(); });
    storage.inicializar();
    network.inicializar();
    network.ativarEthernetFisica();
    localTime.carregarEConfigurarHorario();

    if (network.conectarWifi())
    {
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
    }
    else
    {
        network.iniciarPortal();
    }
    oled.paginasAutomaticas();
    weather.begin();
    if (xTaskCreatePinnedToCore(applicationLoop, "device_app", 12288, nullptr, 1, &applicationTask,
                                1) != pdPASS)
    {
        applicationTask = nullptr;
        Serial.println(F("[CORES] Falha ao criar tarefa principal; usando loop Arduino."));
    }
}

void loop()
{
    if (!applicationTask)
        executarCiclo();
    delay(10);
}

void executarCiclo()
{
    network.processarPortalERedes();
    pageAudio.suspend(network.atualizacaoEmCurso());
    weather.suspend(network.atualizacaoEmCurso());
    if (network.atualizacaoEmCurso())
        return;
    cmdHandler.processarSerial();
    cmdHandler.atualizar();

    oled.atualizarPainel(localTime, network, storage);

    WiFiUDP& udp = network.getUdpDriver();
    int packetSize = udp.parsePacket();
    if (packetSize)
    {
        char packetBuffer[255];
        if (packetSize >= int(sizeof(packetBuffer)))
        {
            while (udp.available())
                udp.read(packetBuffer, sizeof(packetBuffer));
            network.responderUDP("Erro: comando excede 254 bytes\n");
            return;
        }
        int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        if (len > 0)
        {
            packetBuffer[len] = 0;
            cmdHandler.executar(String(packetBuffer));
        }
    }

    // Navigation only. A stable falling edge wakes and advances once; holding never erases Wi-Fi.
    static int readingBefore = HIGH, stable = HIGH;
    static uint32_t changedAt = 0;
    int reading = digitalRead(BOTAO_RESET);
    if (reading != readingBefore)
        changedAt = millis();
    if (millis() - changedAt >= 50 && reading != stable)
    {
        stable = reading;
        if (stable == LOW)
            oled.avancarPagina();
    }
    readingBefore = reading;
}
