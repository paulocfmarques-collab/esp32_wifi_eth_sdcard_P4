#include "SlaveCommandHandler.h"
#include "SD_MMC.h"
#include <esp_system.h>
#include "CrashDiagnostics.h"
#include "CpuTemperature.h"
#include "PageAudio.h"
#include "WeatherService.h"
#include "MqttService.h"
#include "EdgeComputing.h"
#include "CameraService.h"
#include "UsbHostService.h"
#include "SystemLog.h"

#include "FirmwareVersion.h"

static String _ultimoComando = "Nenhum";
static uint32_t _totalComandos = 0;

static String normalizarCaminho(String c)
{
    c.trim();
    if (!c.startsWith("/"))
        c = "/" + c;
    return c;
}

static const char* motivoReset()
{
    switch (esp_reset_reason())
    {
    case ESP_RST_POWERON:
        return "Power-on";
    case ESP_RST_EXT:
        return "Reset externo";
    case ESP_RST_SW:
        return "Reset por software";
    case ESP_RST_PANIC:
        return "Panic";
    case ESP_RST_INT_WDT:
        return "Watchdog interrupcao";
    case ESP_RST_TASK_WDT:
        return "Watchdog tarefa";
    case ESP_RST_WDT:
        return "Watchdog";
    case ESP_RST_DEEPSLEEP:
        return "Deep sleep";
    case ESP_RST_BROWNOUT:
        return "Brownout";
    default:
        return "Desconhecido";
    }
}

SlaveCommandHandler::SlaveCommandHandler(OledDisplay& o, StorageManager& s, NetworkController& n,
                                         TimeManager& t)
    : _oled(o), _storage(s), _net(n), _time(t)
{
}

void SlaveCommandHandler::executar(String cmd, NetworkController::CommandChannel channel)
{
    const auto previous = _net.commandChannel;
    _net.commandChannel = channel;
    executarInterno(cmd);
    _net.commandChannel = previous;
}

void SlaveCommandHandler::iniciarSerial()
{
    _serialQueue = xQueueCreate(8, sizeof(SerialMessage));
    if (!_serialQueue ||
        xTaskCreatePinnedToCore(serialTask, "serial_rx", 4096, this, 1, &_serialTask, 0) != pdPASS)
    {
        if (_serialQueue)
        {
            vQueueDelete(_serialQueue);
            _serialQueue = nullptr;
        }
        Serial.println(F("[CORES] Falha ao criar tarefa serial; usando loop principal."));
    }
    else
        Serial.println(F("[CORES] Recepcao serial no core 0; fila de 8 comandos."));
}

void SlaveCommandHandler::serialTask(void* context)
{
    auto* handler = static_cast<SlaveCommandHandler*>(context);
    for (;;)
    {
        handler->lerSerial();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void SlaveCommandHandler::processarSerial()
{
    if (!_serialQueue)
    {
        lerSerial();
        return;
    }
    uint32_t dropped = _serialDropped.exchange(0);
    if (dropped)
        Serial.println("Erro: fila serial cheia; comandos descartados: " + String(dropped));
    SerialMessage message;
    if (xQueueReceive(_serialQueue, &message, 0) == pdTRUE)
    {
        if (message.overflow)
            Serial.println(F("Erro: comando excede 254 bytes"));
        else
            executar(String(message.command), NetworkController::CommandChannel::ConsoleSerial);
    }
}

void SlaveCommandHandler::lerSerial()
{
    // Bound each pass so a continuous serial stream cannot starve the portal.
    for (int budget = 0; budget < 512 && Serial.available(); ++budget)
    {
        const char c = char(Serial.read());
        if (c == '\r' || c == '\n')
        {
            if (_serialOverflow || _serialLength)
            {
                _serialBuffer[_serialLength] = '\0';
                if (_serialQueue)
                {
                    SerialMessage message = {};
                    message.overflow = _serialOverflow;
                    memcpy(message.command, _serialBuffer, _serialLength + 1);
                    if (xQueueSend(_serialQueue, &message, 0) != pdTRUE)
                        _serialDropped.fetch_add(1);
                }
                else if (_serialOverflow)
                    Serial.println(F("Erro: comando excede 254 bytes"));
                else
                    executar(String(_serialBuffer),
                             NetworkController::CommandChannel::ConsoleSerial);
            }
            _serialLength = 0;
            _serialOverflow = false;
        }
        else if (!_serialOverflow)
        {
            if (_serialLength < sizeof(_serialBuffer) - 1)
                _serialBuffer[_serialLength++] = c;
            else
                _serialOverflow = true; // Discard the entire line, never execute a prefix.
        }
    }
}

void SlaveCommandHandler::executarInterno(String cmd)
{
    // Preserve password whitespace; trim only the command name below.
    while (cmd.endsWith("\r") || cmd.endsWith("\n"))
        cmd.remove(cmd.length() - 1);

    // Nome do comando em minusculas; o argumento (apos ':') preserva a caixa original
    String arg = "";
    String nome = cmd;
    int sep = cmd.indexOf(':');
    if (sep != -1)
    {
        nome = cmd.substring(0, sep);
        arg = cmd.substring(sep + 1);
    }
    nome.trim();
    nome.toLowerCase();
    if (nome.isEmpty())
        return;

    // Apelidos dos nomes antigos
    if (nome == "init")
        nome = "reason";
    else if (nome == "flash_info")
        nome = "flash";
    else if (nome == "restart")
        nome = "reboot";
    else if (nome == "list")
        nome = "sd_list";
    else if (nome == "read")
        nome = "sd_read";
    else if (nome == "write")
        nome = "sd_write";
    else if (nome == "delete")
        nome = "sd_del";
    else if (nome == "exists")
        nome = "sd_exists";
    else if (nome == "size")
        nome = "sd_size";
    else if (nome == "tail")
        nome = "sd_tail";
    else if (nome == "sd_info")
        nome = "sd_status";
    else if (nome == "set_tz")
        nome = "set_fuso";
    else if (nome == "set_dst")
        nome = "dst";

    String visible = nome == "wifi_add" ? String("wifi_add:[credenciais ocultas]") : cmd;
    if (nome != "lastcmd" && nome != "cmdcount")
    {
        _ultimoComando = visible;
        _totalComandos++;
    }

    // Background network/SD logs and scrapes must not reset the idle timer.
    // A status query for the saver must also leave its state unchanged.
    if (!(nome == "protetor" && arg == "status")) _oled.registrarAtividade();
    _oled.adicionarLinha("> " + visible);
    _storage.gravarLog("> " + visible);

    char* pMsg = (char*)calloc(512, sizeof(char));
    if (!pMsg)
    {
        _net.responderUDP("Erro: memoria insuficiente\n");
        return;
    }

    const bool sdPrecisa = nome.startsWith("sd_");
    if (sdPrecisa && !_storage.isAtivo())
    {
        _storage.gravarLog("[ERRO] Comando " + nome + " rejeitado: Cartao SD inacessivel.");
        _net.responderUDP("Erro: SD inacessivel\n");
        free(pMsg);
        return;
    }

    // ─── Sistema ───
    if (nome == "lastcmd")
    {
        _net.responderUDP("Ultimo Comando: " + _ultimoComando + "\n");
    }
    else if (nome == "net_monitor")
    {
        _net.responderUDP(_net.monitor().report());
    }
    else if (nome == "tela")
    {
        if (arg == "auto")
        {
            _oled.paginasAutomaticas();
            _net.responderUDP("Telas automaticas (8 s)\n");
        }
        else if (arg == "next")
        {
            _oled.avancarPagina();
            _net.responderUDP("Proxima tela\n");
        }
        else if (arg.length() == 1 && arg[0] >= '0' && arg[0] <= '5')
        {
            _oled.selecionarPagina(arg.toInt());
            _net.responderUDP("Tela selecionada\n");
        }
        else
            _net.responderUDP("Use tela:0..5, tela:next ou tela:auto\n");
    }
    else if (nome == "protetor")
    {
        if (arg == "on") _oled.iniciarProtetor();
        else if (arg == "off") _oled.configurarProtetor(false);
        else if (arg == "auto") _oled.configurarProtetor(true);
        else if (arg.startsWith("tempo:")) {
            String seconds = arg.substring(6);
            bool valid = seconds.length() > 0 && seconds.length() <= 4;
            for (char c : seconds) if (c < '0' || c > '9') valid = false;
            long value = seconds.toInt();
            if (!valid || value < 5 || value > 3600) {
                _net.responderUDP("Use protetor:tempo:5..3600 (segundos)\n");
                free(pMsg); return;
            }
            _oled.tempoProtetor(value);
        }
        else if (arg != "status") {
            _net.responderUDP("Use protetor:on, off, auto, status ou tempo:SEGUNDOS\n");
            free(pMsg); return;
        }
        _net.responderUDP(_oled.statusProtetor());
    }
    else if (nome == "usb_status") { _net.responderUDP(usbHost.status() + "\n"); }
    else if (nome == "usb_list") { _net.responderUDP(usbHost.files()); }
    else if (nome == "usb_rescan") { usbHost.rescan(); _net.responderUDP("Releitura da raiz solicitada; consulte usb_list em instantes\n"); }
    else if (nome == "log_status") { _net.responderUDP(systemLog.status() + "\n"); }
    else if (nome == "camera_oled")
    {
        uint8_t pixels[1024];
        if (!arg.isEmpty()) _net.responderUDP("Use camera_oled sem argumentos\n");
        else if (!camera.copyOled(pixels, sizeof(pixels))) _net.responderUDP("Camera sem imagem recente; consulte camera_status\n");
        else if (!_oled.mostrarImagemCamera(pixels, sizeof(pixels))) _net.responderUDP("OLED indisponivel ou ocupado\n");
        else _net.responderUDP("Quadro da camera no OLED por 10 s. Botao ou tela:next para sair.\n");
    }
    else if (nome == "camera_status")
    {
        _net.responderUDP(camera.status() + "\n");
    }
    else if (nome == "edge_status")
    {
        _net.responderUDP(edge.status() + "\n");
    }
    else if (nome == "mqtt_status")
    {
        _net.responderUDP(mqtt.status());
    }
    else if (nome == "weather_status")
    {
        _net.responderUDP(weather.status());
    }
    else if (nome == "audio_test")
    {
        if (!arg.isEmpty()) _net.responderUDP("Use audio_test sem argumentos\n");
        else _net.responderUDP(pageAudio.play(true) ? "Teste de 500 ms solicitado. Consulte audio_status depois.\n" :
                                                    "Audio indisponivel ou suspenso. Consulte audio_status.\n");
    }
    else if (nome == "audio_status")
    {
        _net.responderUDP(pageAudio.status());
    }
    else if (nome == "botao_status")
    {
        _net.responderUDP(
            "Botao configurado: GPIO " + String(BOTAO_RESET) + "\nLeitura: " +
            String(digitalRead(BOTAO_RESET) == LOW ? "LOW (pressionado)" : "HIGH (solto)") +
            "\nLigacao esperada: botao entre o GPIO configurado e GND (INPUT_PULLUP)\n");
    }
    else if (nome == "cmdcount")
    {
        sprintf(pMsg, "Total Comandos: %u\n", (unsigned)_totalComandos);
        _net.responderUDP(pMsg);
    }
    else if (nome == "alive")
    {
        _net.responderUDP("alive\n");
    }
    else if (nome == "cores")
    {
        _net.responderUDP(
            "Execucao de comandos: core " + String(xPortGetCoreID()) +
            "\nRecepcao serial: " + String(_serialTask ? "core 0" : "fallback no loop") +
            "\nFila serial: " + String(_serialQueue ? uxQueueMessagesWaiting(_serialQueue) : 0) +
            " de 8\n");
    }
    else if (nome == "ota_info")
    {
        _net.responderUDP(_net.statusOTA());
    }
    else if (nome == "ota_password")
    {
        if (_net.commandChannel == NetworkController::CommandChannel::ConsoleSerial)
            _net.imprimirCredenciaisOTA();
        else
            _net.responderUDP("Credenciais OTA disponiveis somente pela serial local\n");
    }
    else if (nome == "version")
    {
        _net.responderUDP("Versao Firmware: " FIRMWARE_VERSION "\n");
    }
    else if (nome == "build")
    {
        sprintf(pMsg, "Compilacao:\nData: %s\nHora: %s\n", __DATE__, __TIME__);
        _net.responderUDP(pMsg);
    }
    else if (nome == "crash_info")
    {
        if (!arg.isEmpty())
        {
            _net.responderUDP("Use crash_info sem argumentos\n");
        }
        else
        {
            String report = CrashDiagnostics::lerArquivarEApagar(_storage, motivoReset());
            if (_net.commandChannel == NetworkController::CommandChannel::UDP)
            {
                // The central receives up to 1459 bytes per packet: do not truncate its report.
                for (size_t start = 0; start < report.length();)
                {
                    size_t end = min(start + 1200, size_t(report.length()));
                    if (end < report.length())
                    {
                        int newline = report.lastIndexOf('\n', end - 1);
                        if (newline >= int(start))
                            end = newline + 1;
                    }
                    _net.responderUDP(report.substring(start, end));
                    start = end;
                }
            }
            else
                _net.responderUDP(report);
        }
    }
    else if (nome == "reason")
    {
        _net.responderUDP(String("Reset: ") + motivoReset() + "\n");
    }
    else if (nome == "info")
    {
        struct tm timeinfo;
        char dataBuf[12] = "Sem Sinc.";
        char horaBuf[10] = "Sem Sinc.";
        if (getLocalTime(&timeinfo, 100))
        {
            sprintf(dataBuf, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1,
                    timeinfo.tm_year + 1900);
            sprintf(horaBuf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
        String ip =
            (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : ETH.localIP().toString();
        snprintf(pMsg, 512,
                 "===== DEVICE INFO =====\n"
                 "Hostname: %s\n"
                 "Firmware: %s\n"
                 "Build: %s %s\n"
                 "SSID: %s\n"
                 "IP: %s\n"
                 "MAC: %s\n"
                 "RSSI: %d dBm\n"
                 "Heap Livre: %u KB\n"
                 "SD Card: %s\n"
                 "Data: %s\n"
                 "Hora: %s\n"
                 "Uptime: %lu ms\n"
                 "=======================\n",
                 ETH.getHostname(), FIRMWARE_VERSION, __DATE__, __TIME__, WiFi.SSID().c_str(),
                 ip.c_str(), WiFi.macAddress().c_str(), WiFi.RSSI(),
                 (unsigned)(ESP.getFreeHeap() / 1024), _storage.isAtivo() ? "OK" : "FALHA", dataBuf,
                 horaBuf, millis());
        _net.responderUDP(pMsg);
    }
    else if (nome == "status")
    {
        struct tm timeinfo;
        bool ntpOk = getLocalTime(&timeinfo, 100) && (timeinfo.tm_year + 1900 > 1970);
        sprintf(pMsg, "ONLINE\nWiFi: %s\nSD: %s\nNTP: %s\nHeap: %u KB\n",
                (WiFi.status() == WL_CONNECTED) ? "OK" : "FALHA",
                _storage.isAtivo() ? "OK" : "FALHA", ntpOk ? "OK" : "FALHA",
                (unsigned)(ESP.getFreeHeap() / 1024));
        _net.responderUDP(pMsg);
    }
    else if (nome == "help")
    {
        _net.responderUDP(
            "COMANDOS\n"
            "help\n"
            "info\n"
            "status\n"
            "version\n"
            "build\n"
            "reason\n"
            "crash_info\n"
            "alive\n"
            "cores\n"
            "ota_info\n"
            "name\n"
            "ota_password\n"
            "uptime\n"
            "lastcmd\n"
            "cmdcount\n"
            "reboot\n"
            "reset_counters\n"
            "health\n"
            "selftest\n"
            "temp\n"
            "cpu\n"
            "chip_info\n"
            "ram\n"
            "heap\n"
            "heap_min\n"
            "psram\n"
            "flash\n"
            "flash_info\n"
            "net_info\n"
            "net_monitor\n"
            "eth_status\n"
            "mac\n"
            "rssi\n"
            "ip\n"
            "ssid\n"
            "channel\n"
            "wifi_status\n"
            "wifi_list\n"
            "wifi_add:SSID|SENHA\n"
            "reset_wifi\n"
            "tela:0..5\n"
            "tela:[next]\n"
            "tela:[auto]\n"
            "botao_status\n"
            "protetor:on|off|auto|status|tempo:SEGUNDOS\n"
            "audio_status\n"
            "audio_test\n"
            "weather_status\n"
            "mqtt_status\n"
            "edge_status\n"
            "camera_status\n"
            "camera_oled\n"
            "usb_status\nusb_list\nusb_rescan\nlog_status\n"
            "time\n"
            "date\n"
            "ntp_status\n"
            "set_fuso:N\n"
            "dst_on\n"
            "dst_off\n"
            "dst:0|1\n"
            "led_on\n"
            "led_off\n"
            "led_blink[:duracao_ms]\n"
            "sd_status\n"
            "sd_list\n"
            "sd_read:arquivo\n"
            "sd_write:arquivo:texto\n"
            "sd_append:arquivo:texto\n"
            "sd_del:arquivo\n"
            "sd_exists:arquivo\n"
            "sd_size:arquivo\n"
            "sd_tail:arquivo[:linhas]\n"
            "sd_log[:linhas]\n"
            "sd_clear_log\n"
            "sd_mkdir:pasta\n"
            "sd_rmdir:pasta\n"
            "init\n"
            "restart\n"
            "list\n"
            "read\n"
            "write\n"
            "delete\n"
            "exists\n"
            "size\n"
            "tail\n"
            "sd_info\n"
            "set_tz\n"
            "set_dst\n");
    }
    else if (nome == "name")
    {
        _net.responderUDP("ESP32P4\n");
    }
    else if (nome == "reset_counters")
    {
        _net.responderUDP(_net.resetCounters()
                              ? "Contadores NVS de boot/reboot/watchdog zerados.\n"
                              : "Erro ao zerar contadores NVS.\n");
    }
    else if (nome == "reset_wifi")
    {
        free(pMsg);
        _net.zerarConfiguracoes();
        return;
    }
    else if (nome == "reboot")
    {
        _oled.adicionarLinha("Reiniciando...");
        _storage.gravarLog("[SISTEMA] Reinicializacao solicitada via comando reboot.");
        _net.responderUDP("Reiniciando...\n");
        free(pMsg);
        _net.agendarReinicio();
        return;
    }
    // ─── Hardware ───
    else if (nome == "temp")
    {
        float temperature = lerTemperaturaInterna();
        if (isfinite(temperature))
            sprintf(pMsg, "Temp: %.2fC\n", temperature);
        else
            strcpy(pMsg, "Temp: indisponivel\n");
        _net.responderUDP(pMsg);
    }
    else if (nome == "cpu")
    {
        sprintf(pMsg, "Modelo: %s\nRevisao: %d\nFreq: %d MHz\n", ESP.getChipModel(),
                ESP.getChipRevision(), ESP.getCpuFreqMHz());
        _net.responderUDP(pMsg);
    }
    else if (nome == "ram")
    {
        sprintf(pMsg, "Heap livre: %u\nMenor heap: %u\nMaior bloco: %u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());
        _net.responderUDP(pMsg);
    }
    else if (nome == "chip_info")
    {
        snprintf(pMsg, 512, "Chip: %s\nRevisao: %u\nNucleos: %u\nCPU: %u MHz\nSDK: %s\n",
                 ESP.getChipModel(), (unsigned)ESP.getChipRevision(), (unsigned)ESP.getChipCores(),
                 (unsigned)ESP.getCpuFreqMHz(), ESP.getSdkVersion());
        _net.responderUDP(pMsg);
    }
    else if (nome == "heap_min")
    {
        _net.responderUDP("Menor heap livre: " + String(ESP.getMinFreeHeap()) + " B\n");
    }
    else if (nome == "psram")
    {
        _net.responderUDP("PSRAM total: " + String(ESP.getPsramSize()) +
                          " B\nPSRAM livre: " + String(ESP.getFreePsram()) + " B\n");
    }
    else if (nome == "health" || nome == "selftest")
    {
        struct tm t;
        bool clock = getLocalTime(&t, 10);
        snprintf(pMsg, 512,
                 "DIAGNOSTICO (somente leitura)\nWiFi: %s\nEthernet: %s\nOLED: %s\nSD montado: "
                 "%s\nRelogio valido: %s\nHeap livre/minimo: %u/%u B\nPSRAM total/livre: %u/%u B\n",
                 WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE",
                 NetworkController::ethConectado ? "ONLINE" : "OFFLINE",
                 _oled.isPronto() ? "OK" : "FALHA", _storage.isAtivo() ? "SIM" : "NAO",
                 clock ? "SIM" : "NAO", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                 (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram());
        _net.responderUDP(pMsg);
    }
    else if (nome == "heap")
    {
        sprintf(pMsg, "Heap livre: %u B\n", (unsigned)ESP.getFreeHeap());
        _net.responderUDP(pMsg);
    }
    else if (nome == "flash")
    {
        sprintf(pMsg, "Flash total: %u\nTamanho Sketch: %u\nEspaco livre: %u\n",
                (unsigned)ESP.getFlashChipSize(), (unsigned)ESP.getSketchSize(),
                (unsigned)ESP.getFreeSketchSpace());
        _net.responderUDP(pMsg);
    }
    else if (nome == "uptime")
    {
        sprintf(pMsg, "Uptime: %lu ms\n", millis());
        _net.responderUDP(pMsg);
    }
    // ─── Rede ───
    else if (nome == "net_info")
    {
        _net.responderUDP(
            "WiFi: " + String(WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE") +
            "\nSSID: " + WiFi.SSID() + "\nIP: " + WiFi.localIP().toString() +
            "\nGateway: " + WiFi.gatewayIP().toString() +
            "\nMascara: " + WiFi.subnetMask().toString() + "\nDNS: " + WiFi.dnsIP().toString() +
            "\nEthernet: " + String(NetworkController::ethConectado ? "ONLINE" : "OFFLINE") +
            "\nIP ETH: " + ETH.localIP().toString() + "\nGateway ETH: " +
            ETH.gatewayIP().toString() + "\nMascara ETH: " + ETH.subnetMask().toString() +
            "\nDNS ETH: " + ETH.dnsIP().toString() + "\n");
    }
    else if (nome == "wifi_list")
    {
        _net.responderUDP(_net.listarSSIDs());
    }
    else if (nome == "wifi_add")
    {
        int divider = arg.indexOf('|');
        String error;
        if (divider <= 0)
            _net.responderUDP("Use wifi_add:SSID|SENHA (senha vazia para rede aberta)\n");
        else if (_net.adicionarRede(arg.substring(0, divider), arg.substring(divider + 1), error))
            _net.responderUDP("Rede salva e verificada. Use reboot para aplicar.\n");
        else
            _net.responderUDP("Erro: " + error + "\n");
    }
    else if (nome == "channel")
    {
        _net.responderUDP("Canal WiFi: " + String(WiFi.channel()) + "\n");
    }
    else if (nome == "wifi_status")
    {
        _net.responderUDP("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE") +
                          "\nStatus: " + String((int)WiFi.status()) +
                          "\nModo: " + String((int)WiFi.getMode()) + "\nSSID: " + WiFi.SSID() +
                          "\nIP: " + WiFi.localIP().toString() +
                          "\nIP AP: " + WiFi.softAPIP().toString() + "\n");
    }
    else if (nome == "eth_status")
    {
        _net.responderUDP(
            "Ethernet: " + String(NetworkController::ethConectado ? "ONLINE" : "OFFLINE") +
            "\nCabo: " + String(ETH.linkUp() ? "conectado" : "desconectado") +
            "\nIP: " + ETH.localIP().toString() + "\nGateway: " + ETH.gatewayIP().toString() +
            "\nMAC: " + ETH.macAddress() + "\n");
    }
    else if (nome == "mac")
    {
        _net.responderUDP("MAC: " + WiFi.macAddress() + "\n");
    }
    else if (nome == "rssi")
    {
        _net.responderUDP("RSSI: " + String(WiFi.RSSI()) + " dBm\n");
    }
    else if (nome == "ip")
    {
        _net.responderUDP("IP: " + WiFi.localIP().toString() +
                          "\nETH: " + ETH.localIP().toString() + "\n");
    }
    else if (nome == "ssid")
    {
        _net.responderUDP("SSID: " + WiFi.SSID() + "\n");
    }
    // ─── Hora ───
    else if (nome == "ntp_status")
    {
        struct tm t;
        bool valid = getLocalTime(&t, 10);
        _net.responderUDP(
            String("Relogio: ") + (valid ? "VALIDO" : "SEM SINCRONIZACAO") +
            "\nHora local: " + _time.obterStringDataHora() +
            "\nServidores configurados: a.st1.ntp.br, pool.ntp.org, time.nist.gov\nRelogio valido "
            "nao confirma alcance atual dos servidores NTP.\n");
    }
    else if (nome == "date")
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 100))
        {
            sprintf(pMsg, "Data:\n%02d/%02d/%04d\n", timeinfo.tm_mday, timeinfo.tm_mon + 1,
                    timeinfo.tm_year + 1900);
            _net.responderUDP(pMsg);
        }
        else
        {
            _storage.gravarLog("[ERRO] Falha ao ler DATE: Relogio sem sincronizacao NTP.");
            _net.responderUDP("Erro: Sem Hora Sinc.\n");
        }
    }
    else if (nome == "time")
    {
        String horaStr = _time.obterApenasHora();
        if (horaStr == "00:00:00")
        {
            _storage.gravarLog("[ERRO] Falha ao ler TIME: Relogio retornou valor nulo/padrao.");
        }
        _net.responderUDP("Hora:\n" + horaStr + "\n");
    }
    else if (nome == "set_fuso")
    {
        String number = arg;
        number.trim();
        bool numeric = number.length() > 0;
        for (size_t i = 0; i < number.length(); i++)
            if (!isDigit(number[i]) &&
                !(i == 0 && number.length() > 1 && (number[i] == '-' || number[i] == '+')))
                numeric = false;
        int fusoHoras = number.toInt();
        if (!numeric || number.length() > 3 || fusoHoras < -12 || fusoHoras > 14)
        {
            _net.responderUDP("Use set_fuso:N com inteiro entre -12 e +14\n");
            free(pMsg);
            return;
        }
        long novoGmtOffset = (long)fusoHoras * 3600;
        _time.atualizarFuso(novoGmtOffset);
        sprintf(pMsg, "Fuso alterado para UTC %d (%ld s)\n", fusoHoras, novoGmtOffset);
        _net.responderUDP(pMsg);
    }
    else if (nome == "dst_on" || nome == "dst_off" || nome == "dst")
    {
        if (nome == "dst" && arg != "0" && arg != "1")
        {
            _net.responderUDP("Use dst:0 ou dst:1\n");
            free(pMsg);
            return;
        }
        int dstHoras = (nome == "dst_on") ? 1 : (nome == "dst_off") ? 0 : arg.toInt();
        _time.atualizarDST(dstHoras * 3600);
        sprintf(pMsg, "Horario de verao: %dh\n", dstHoras);
        _net.responderUDP(pMsg);
    }
    // ─── LED ───
    else if (nome == "led_on")
    {
        pararLedBlink();
        digitalWrite(LED_PIN, HIGH);
        _net.responderUDP("LED ligado\n");
    }
    else if (nome == "led_off")
    {
        pararLedBlink();
        digitalWrite(LED_PIN, LOW);
        _net.responderUDP("LED desligado\n");
    }
    else if (nome == "led_blink")
    {
        long ms = arg.toInt();
        if (ms <= 0)
            ms = 3000;
        if (ms > 10000)
            ms = 10000;
        _net.responderUDP("LED piscando\n");
        _ledStarted = _ledChanged = millis();
        _ledDuration = uint32_t(ms);
        _ledBlinking = true;
        digitalWrite(LED_PIN, HIGH);
    }
    // ─── SD ───
    else if (nome == "sd_status")
    {
        uint64_t total = SD_MMC.totalBytes();
        uint64_t usado = SD_MMC.usedBytes();
        sprintf(pMsg,
                "--- Status SD Card ---\nTotal: %u MB\nUsado: %u MB\nLivre: %u "
                "MB\n----------------------\n",
                (unsigned)(total / (1024 * 1024)), (unsigned)(usado / (1024 * 1024)),
                (unsigned)((total - usado) / (1024 * 1024)));
        _net.responderUDP(pMsg);
    }
    else if (nome == "sd_list")
    {
        File raiz = SD_MMC.open("/");
        File arquivo = raiz.openNextFile();
        if (!arquivo)
        {
            _net.responderUDP("Diretorio vazio\n");
        }
        else
        {
            _net.responderUDP("--- Arquivos SD ---\n");
            while (arquivo)
            {
                _net.responderUDP(String(arquivo.name()) + " (" + String(arquivo.size()) + " B)\n");
                arquivo = raiz.openNextFile();
            }
            _net.responderUDP("-------------------\n");
        }
        raiz.close();
    }
    else if (nome == "sd_read" || nome == "sd_log" || nome == "sd_tail")
    {
        String caminho;
        int linhasDesejadas = 0;
        if (nome == "sd_log")
        {
            caminho = "/log.txt";
            linhasDesejadas = arg.toInt();
            if (linhasDesejadas <= 0)
                linhasDesejadas = 10;
        }
        else if (nome == "sd_tail")
        {
            linhasDesejadas = 10;
            int i = arg.indexOf(':');
            if (i != -1)
            {
                caminho = normalizarCaminho(arg.substring(0, i));
                linhasDesejadas = arg.substring(i + 1).toInt();
                if (linhasDesejadas <= 0)
                    linhasDesejadas = 10;
            }
            else
            {
                caminho = normalizarCaminho(arg);
            }
        }
        else
        {
            caminho = normalizarCaminho(arg);
        }

        if (!SD_MMC.exists(caminho))
        {
            _net.responderUDP("Arquivo nao existe\n");
        }
        else
        {
            File arquivo = SD_MMC.open(caminho, FILE_READ);
            if (!arquivo || arquivo.isDirectory())
            {
                _storage.gravarLog("[ERRO] Falha ao abrir para leitura: " + caminho);
                _net.responderUDP("Falha ao abrir arquivo\n");
            }
            else
            {
                if (linhasDesejadas > 0)
                {
                    int total = 0;
                    while (arquivo.available())
                    {
                        arquivo.readStringUntil('\n');
                        total++;
                    }
                    int inicial = total - linhasDesejadas;
                    if (inicial < 0)
                        inicial = 0;
                    arquivo.seek(0);
                    for (int i = 0; i < inicial && arquivo.available(); i++)
                        arquivo.readStringUntil('\n');
                    _net.responderUDP("--- TAIL: " + caminho + " ---\n");
                    while (arquivo.available())
                        _net.responderUDP(arquivo.readStringUntil('\n') + "\n");
                    _net.responderUDP("--- Fim do Bloco ---\n");
                }
                else
                {
                    _net.responderUDP("--- Lendo: " + caminho + " ---\n");
                    uint32_t tamanhoTotal = arquivo.size();
                    uint32_t bytesLidos = 0;
                    int ultimaPorcentagem = -1;
                    _oled.configurarProgresso(true, 0);
                    while (arquivo.available())
                    {
                        String linha = arquivo.readStringUntil('\n') + "\n";
                        bytesLidos += linha.length();
                        _net.responderUDP(linha);
                        int pct = (tamanhoTotal > 0) ? (bytesLidos * 100 / tamanhoTotal) : 100;
                        if (pct != ultimaPorcentagem)
                        {
                            ultimaPorcentagem = pct;
                            _oled.configurarProgresso(true, pct);
                        }
                    }
                    _net.responderUDP("--- Fim do Arquivo ---\n");
                    _oled.configurarProgresso(false, 0);
                }
                arquivo.close();
            }
        }
    }
    else if (nome == "sd_write" || nome == "sd_append")
    {
        arg.trim();
        // Aceita "arquivo:conteudo" (padrao) ou "arquivo=conteudo" (formato antigo)
        int ic = arg.indexOf(':');
        int ie = arg.indexOf('=');
        int i = (ic == -1) ? ie : (ie == -1 ? ic : min(ic, ie));
        if (i == -1)
        {
            _net.responderUDP("Erro: Formato invalido. Use sd_write:/arquivo.txt:conteudo ou "
                              "sd_append:/arquivo.txt:conteudo\n");
        }
        else
        {
            String caminho = normalizarCaminho(arg.substring(0, i));
            String conteudo = arg.substring(i + 1);
            File arquivo = SD_MMC.open(caminho, nome == "sd_append" ? FILE_APPEND : FILE_WRITE);
            if (!arquivo)
            {
                _storage.gravarLog("[ERRO] Falha ao abrir para escrita: " + caminho);
                _net.responderUDP("Erro ao abrir arquivo\n");
            }
            else
            {
                size_t written =
                    nome == "sd_append" ? arquivo.println(conteudo) : arquivo.print(conteudo);
                arquivo.close();
                size_t expected = conteudo.length() + (nome == "sd_append" ? 2 : 0);
                if (written != expected)
                {
                    _net.responderUDP("Erro: gravacao incompleta\n");
                    free(pMsg);
                    return;
                }
                _storage.gravarLog("[SD] Arquivo gravado com sucesso: " + caminho);
                _net.responderUDP("Ok\n");
            }
        }
    }
    else if (nome == "sd_mkdir" || nome == "sd_rmdir")
    {
        String caminho = normalizarCaminho(arg);
        if (arg.isEmpty() || caminho == "/")
            _net.responderUDP("Informe uma pasta diferente da raiz\n");
        else
        {
            bool ok = nome == "sd_mkdir" ? SD_MMC.mkdir(caminho) : SD_MMC.rmdir(caminho);
            _net.responderUDP(
                ok ? "Ok\n" : "Falha: verifique o caminho e se a pasta esta vazia ao remover\n");
        }
    }
    else if (nome == "sd_del")
    {
        String caminho = normalizarCaminho(arg);
        if (!SD_MMC.exists(caminho))
        {
            _net.responderUDP("Arquivo nao existe\n");
        }
        else if (SD_MMC.remove(caminho))
        {
            _storage.gravarLog("[SD] Arquivo excluido com sucesso: " + caminho);
            _net.responderUDP("Ok\n");
        }
        else
        {
            _storage.gravarLog("[ERRO] Falha ao excluir: " + caminho);
            _net.responderUDP("Erro ao deletar arquivo\n");
        }
    }
    else if (nome == "sd_exists")
    {
        _net.responderUDP(SD_MMC.exists(normalizarCaminho(arg)) ? "Existe\n" : "Nao existe\n");
    }
    else if (nome == "sd_size")
    {
        String caminho = normalizarCaminho(arg);
        if (!SD_MMC.exists(caminho))
        {
            _net.responderUDP("Arquivo nao existe\n");
        }
        else
        {
            File arquivo = SD_MMC.open(caminho, FILE_READ);
            if (!arquivo || arquivo.isDirectory())
            {
                _net.responderUDP("Falha ao abrir arquivo\n");
            }
            else
            {
                sprintf(pMsg, "Tamanho: %u B\n", (unsigned)arquivo.size());
                arquivo.close();
                _net.responderUDP(pMsg);
            }
        }
    }
    else if (nome == "sd_clear_log")
    {
        if (SD_MMC.exists("/log.txt"))
            SD_MMC.remove("/log.txt");
        _net.responderUDP("Log limpo\n");
    }
    else
    {
        _storage.gravarLog("[ERRO] Comando recebido invalido ou corrompido: " + cmd);
        _net.responderUDP("Comando Invalido\n");
    }

    free(pMsg);
}

float SlaveCommandHandler::lerTemperaturaInterna()
{
    return readCpuTemperatureCelsius();
}

void SlaveCommandHandler::pararLedBlink()
{
    _ledBlinking = false;
}

void SlaveCommandHandler::atualizar()
{
    if (!_ledBlinking)
        return;
    uint32_t now = millis();
    if (uint32_t(now - _ledStarted) >= _ledDuration)
    {
        _ledBlinking = false;
        digitalWrite(LED_PIN, LOW);
        return;
    }
    if (uint32_t(now - _ledChanged) >= 200)
    {
        _ledChanged = now;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
}
