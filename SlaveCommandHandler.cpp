#include "SlaveCommandHandler.h"
#include "SD_MMC.h"
#include <esp_system.h>

#define FIRMWARE_VERSION "v1.4.3"

static String _ultimoComando = "Nenhum";
static uint32_t _totalComandos = 0;

static String normalizarCaminho(String c) {
  c.trim();
  if (!c.startsWith("/")) c = "/" + c;
  return c;
}

static const char* motivoReset() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "Power-on";
    case ESP_RST_EXT:       return "Reset externo";
    case ESP_RST_SW:        return "Reset por software";
    case ESP_RST_PANIC:     return "Panic";
    case ESP_RST_INT_WDT:   return "Watchdog interrupcao";
    case ESP_RST_TASK_WDT:  return "Watchdog tarefa";
    case ESP_RST_WDT:       return "Watchdog";
    case ESP_RST_DEEPSLEEP: return "Deep sleep";
    case ESP_RST_BROWNOUT:  return "Brownout";
    default:                return "Desconhecido";
  }
}

SlaveCommandHandler::SlaveCommandHandler(OledDisplay& o, StorageManager& s, NetworkController& n, TimeManager& t)
  : _oled(o), _storage(s), _net(n), _time(t) {}

void SlaveCommandHandler::executar(String cmd) {
  cmd.trim();

  // Nome do comando em minusculas; o argumento (apos ':') preserva a caixa original
  String arg = "";
  String nome = cmd;
  int sep = cmd.indexOf(':');
  if (sep != -1) {
    nome = cmd.substring(0, sep);
    arg = cmd.substring(sep + 1);
  }
  nome.trim();
  nome.toLowerCase();

  // Apelidos dos nomes antigos
  if (nome == "restart") nome = "reboot";
  else if (nome == "list") nome = "sd_list";
  else if (nome == "read") nome = "sd_read";
  else if (nome == "write") nome = "sd_write";
  else if (nome == "delete") nome = "sd_del";
  else if (nome == "exists") nome = "sd_exists";
  else if (nome == "size") nome = "sd_size";
  else if (nome == "tail") nome = "sd_tail";
  else if (nome == "sd_info") nome = "sd_status";
  else if (nome == "set_tz") nome = "set_fuso";
  else if (nome == "set_dst") nome = "dst";

  if (nome != "lastcmd" && nome != "cmdcount") {
    _ultimoComando = cmd;
    _totalComandos++;
  }

  _oled.adicionarLinha("> " + cmd);
  _storage.gravarLog("> " + cmd);

  char* pMsg = (char*)calloc(512, sizeof(char));
  if (!pMsg) return;

  const bool sdPrecisa = nome.startsWith("sd_");
  if (sdPrecisa && !_storage.isAtivo()) {
    _storage.gravarLog("[ERRO] Comando " + nome + " rejeitado: Cartao SD inacessivel.");
    _net.responderUDP("Erro: SD inacessivel\n");
    free(pMsg);
    return;
  }

  // ─── Sistema ───
  if (nome == "lastcmd") {
    _net.responderUDP("Ultimo Comando: " + _ultimoComando + "\n");
  }
  else if (nome == "cmdcount") {
    sprintf(pMsg, "Total Comandos: %u\n", (unsigned)_totalComandos);
    _net.responderUDP(pMsg);
  }
  else if (nome == "alive") {
    _net.responderUDP("ALIVE\n");
  }
  else if (nome == "version") {
    _net.responderUDP("Versao Firmware: " FIRMWARE_VERSION "\n");
  }
  else if (nome == "build") {
    sprintf(pMsg, "Compilacao:\nData: %s\nHora: %s\n", __DATE__, __TIME__);
    _net.responderUDP(pMsg);
  }
  else if (nome == "reason") {
    _net.responderUDP(String("Reset: ") + motivoReset() + "\n");
  }
  else if (nome == "info") {
    struct tm timeinfo;
    char dataBuf[12] = "Sem Sinc.";
    char horaBuf[10] = "Sem Sinc.";
    if (getLocalTime(&timeinfo, 100)) {
      sprintf(dataBuf, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      sprintf(horaBuf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : ETH.localIP().toString();
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
             ETH.getHostname(), FIRMWARE_VERSION, __DATE__, __TIME__,
             WiFi.SSID().c_str(), ip.c_str(), WiFi.macAddress().c_str(), WiFi.RSSI(),
             (unsigned)(ESP.getFreeHeap() / 1024), _storage.isAtivo() ? "OK" : "FALHA",
             dataBuf, horaBuf, millis());
    _net.responderUDP(pMsg);
  }
  else if (nome == "status") {
    struct tm timeinfo;
    bool ntpOk = getLocalTime(&timeinfo, 100) && (timeinfo.tm_year + 1900 > 1970);
    sprintf(pMsg, "ONLINE\nWiFi: %s\nSD: %s\nNTP: %s\nHeap: %u KB\n",
            (WiFi.status() == WL_CONNECTED) ? "OK" : "FALHA",
            _storage.isAtivo() ? "OK" : "FALHA",
            ntpOk ? "OK" : "FALHA",
            (unsigned)(ESP.getFreeHeap() / 1024));
    _net.responderUDP(pMsg);
  }
  else if (nome == "help") {
    _net.responderUDP(
      "Sistema: info status version build reason help reboot reset_wifi alive lastcmd cmdcount\n"
      "Hardware: temp cpu ram heap flash uptime\n"
      "Rede: net_info mac rssi ip ssid\n"
      "Hora: time date set_fuso:N dst_on dst_off\n"
      "LED: led_on led_off led_blink[:ms]\n"
      "SD: sd_status sd_list sd_read:arq sd_write:arq:txt sd_del:arq sd_exists:arq sd_size:arq sd_tail:arq[:n] sd_log sd_clear_log\n");
  }
  else if (nome == "reset_wifi") {
    free(pMsg);
    _net.zerarConfiguracoes();
    return;
  }
  else if (nome == "reboot") {
    _oled.adicionarLinha("Reiniciando...");
    _storage.gravarLog("[SISTEMA] Reinicializacao solicitada via comando reboot.");
    _net.responderUDP("Reiniciando...\n");
    delay(1000);
    free(pMsg);
    ESP.restart();
    return;
  }
  // ─── Hardware ───
  else if (nome == "temp") {
    sprintf(pMsg, "Temp: %.2fC\n", lerTemperaturaInterna());
    _net.responderUDP(pMsg);
  }
  else if (nome == "cpu") {
    sprintf(pMsg, "Modelo: %s\nRevisao: %d\nFreq: %d MHz\n",
            ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz());
    _net.responderUDP(pMsg);
  }
  else if (nome == "ram") {
    sprintf(pMsg, "Heap livre: %u\nMenor heap: %u\nMaior bloco: %u\n",
            (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    _net.responderUDP(pMsg);
  }
  else if (nome == "heap") {
    sprintf(pMsg, "Heap livre: %u B\n", (unsigned)ESP.getFreeHeap());
    _net.responderUDP(pMsg);
  }
  else if (nome == "flash") {
    sprintf(pMsg, "Flash total: %u\nTamanho Sketch: %u\nEspaco livre: %u\n",
            (unsigned)ESP.getFlashChipSize(), (unsigned)ESP.getSketchSize(), (unsigned)ESP.getFreeSketchSpace());
    _net.responderUDP(pMsg);
  }
  else if (nome == "uptime") {
    sprintf(pMsg, "Uptime: %lu ms\n", millis());
    _net.responderUDP(pMsg);
  }
  // ─── Rede ───
  else if (nome == "net_info") {
    _net.responderUDP("NET:\nIP: " + WiFi.localIP().toString() + "\nETH: " + ETH.localIP().toString() +
                      "\nRSSI: " + String(WiFi.RSSI()) + " dBm\nSSID: " + WiFi.SSID() + "\n");
  }
  else if (nome == "mac") {
    _net.responderUDP("MAC: " + WiFi.macAddress() + "\n");
  }
  else if (nome == "rssi") {
    _net.responderUDP("RSSI: " + String(WiFi.RSSI()) + " dBm\n");
  }
  else if (nome == "ip") {
    _net.responderUDP("IP: " + WiFi.localIP().toString() + "\nETH: " + ETH.localIP().toString() + "\n");
  }
  else if (nome == "ssid") {
    _net.responderUDP("SSID: " + WiFi.SSID() + "\n");
  }
  // ─── Hora ───
  else if (nome == "date") {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 100)) {
      sprintf(pMsg, "Data:\n%02d/%02d/%04d\n", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      _net.responderUDP(pMsg);
    } else {
      _storage.gravarLog("[ERRO] Falha ao ler DATE: Relogio sem sincronizacao NTP.");
      _net.responderUDP("Erro: Sem Hora Sinc.\n");
    }
  }
  else if (nome == "time") {
    String horaStr = _time.obterApenasHora();
    if (horaStr == "00:00:00") {
      _storage.gravarLog("[ERRO] Falha ao ler TIME: Relogio retornou valor nulo/padrao.");
    }
    _net.responderUDP("Hora:\n" + horaStr + "\n");
  }
  else if (nome == "set_fuso") {
    int fusoHoras = arg.toInt();
    long novoGmtOffset = (long)fusoHoras * 3600;
    _time.atualizarFuso(novoGmtOffset);
    sprintf(pMsg, "Fuso alterado para UTC %d (%ld s)\n", fusoHoras, novoGmtOffset);
    _net.responderUDP(pMsg);
  }
  else if (nome == "dst_on" || nome == "dst_off" || nome == "dst") {
    int dstHoras = (nome == "dst_on") ? 1 : (nome == "dst_off") ? 0 : arg.toInt();
    _time.atualizarDST(dstHoras * 3600);
    sprintf(pMsg, "Horario de verao: %dh\n", dstHoras);
    _net.responderUDP(pMsg);
  }
  // ─── LED ───
  else if (nome == "led_on") {
    digitalWrite(LED_PIN, HIGH);
    _net.responderUDP("LED ligado\n");
  }
  else if (nome == "led_off") {
    digitalWrite(LED_PIN, LOW);
    _net.responderUDP("LED desligado\n");
  }
  else if (nome == "led_blink") {
    long ms = arg.toInt();
    if (ms <= 0) ms = 3000;
    if (ms > 10000) ms = 10000;
    _net.responderUDP("LED piscando\n");
    unsigned long ini = millis();
    while (millis() - ini < (unsigned long)ms) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
    digitalWrite(LED_PIN, LOW);
  }
  // ─── SD ───
  else if (nome == "sd_status") {
    uint64_t total = SD_MMC.totalBytes();
    uint64_t usado = SD_MMC.usedBytes();
    sprintf(pMsg, "--- Status SD Card ---\nTotal: %u MB\nUsado: %u MB\nLivre: %u MB\n----------------------\n",
            (unsigned)(total / (1024 * 1024)), (unsigned)(usado / (1024 * 1024)),
            (unsigned)((total - usado) / (1024 * 1024)));
    _net.responderUDP(pMsg);
  }
  else if (nome == "sd_list") {
    File raiz = SD_MMC.open("/");
    File arquivo = raiz.openNextFile();
    if (!arquivo) {
      _net.responderUDP("Diretorio vazio\n");
    } else {
      _net.responderUDP("--- Arquivos SD ---\n");
      while (arquivo) {
        _net.responderUDP(String(arquivo.name()) + " (" + String(arquivo.size()) + " B)\n");
        arquivo = raiz.openNextFile();
      }
      _net.responderUDP("-------------------\n");
    }
    raiz.close();
  }
  else if (nome == "sd_read" || nome == "sd_log" || nome == "sd_tail") {
    String caminho;
    int linhasDesejadas = 0;
    if (nome == "sd_log") {
      caminho = "/log.txt";
      linhasDesejadas = arg.toInt();
      if (linhasDesejadas <= 0) linhasDesejadas = 10;
    } else if (nome == "sd_tail") {
      linhasDesejadas = 10;
      int i = arg.indexOf(':');
      if (i != -1) {
        caminho = normalizarCaminho(arg.substring(0, i));
        linhasDesejadas = arg.substring(i + 1).toInt();
        if (linhasDesejadas <= 0) linhasDesejadas = 10;
      } else {
        caminho = normalizarCaminho(arg);
      }
    } else {
      caminho = normalizarCaminho(arg);
    }

    if (!SD_MMC.exists(caminho)) {
      _net.responderUDP("Arquivo nao existe\n");
    } else {
      File arquivo = SD_MMC.open(caminho, FILE_READ);
      if (!arquivo || arquivo.isDirectory()) {
        _storage.gravarLog("[ERRO] Falha ao abrir para leitura: " + caminho);
        _net.responderUDP("Falha ao abrir arquivo\n");
      } else {
        if (linhasDesejadas > 0) {
          int total = 0;
          while (arquivo.available()) { arquivo.readStringUntil('\n'); total++; }
          int inicial = total - linhasDesejadas;
          if (inicial < 0) inicial = 0;
          arquivo.seek(0);
          for (int i = 0; i < inicial && arquivo.available(); i++) arquivo.readStringUntil('\n');
          _net.responderUDP("--- TAIL: " + caminho + " ---\n");
          while (arquivo.available()) _net.responderUDP(arquivo.readStringUntil('\n') + "\n");
          _net.responderUDP("--- Fim do Bloco ---\n");
        } else {
          _net.responderUDP("--- Lendo: " + caminho + " ---\n");
          uint32_t tamanhoTotal = arquivo.size();
          uint32_t bytesLidos = 0;
          int ultimaPorcentagem = -1;
          _oled.configurarProgresso(true, 0);
          while (arquivo.available()) {
            String linha = arquivo.readStringUntil('\n') + "\n";
            bytesLidos += linha.length();
            _net.responderUDP(linha);
            int pct = (tamanhoTotal > 0) ? (bytesLidos * 100 / tamanhoTotal) : 100;
            if (pct != ultimaPorcentagem) {
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
  else if (nome == "sd_write") {
    arg.trim();
    // Aceita "arquivo:conteudo" (padrao) ou "arquivo=conteudo" (formato antigo)
    int ic = arg.indexOf(':');
    int ie = arg.indexOf('=');
    int i = (ic == -1) ? ie : (ie == -1 ? ic : min(ic, ie));
    if (i == -1) {
      _net.responderUDP("Erro: Formato invalido. Use sd_write:/arquivo.txt:conteudo\n");
    } else {
      String caminho = normalizarCaminho(arg.substring(0, i));
      String conteudo = arg.substring(i + 1);
      File arquivo = SD_MMC.open(caminho, FILE_WRITE);
      if (!arquivo) {
        _storage.gravarLog("[ERRO] Falha ao abrir para escrita: " + caminho);
        _net.responderUDP("Erro ao abrir arquivo\n");
      } else {
        arquivo.print(conteudo);
        arquivo.close();
        _storage.gravarLog("[SD] Arquivo gravado com sucesso: " + caminho);
        _net.responderUDP("Ok\n");
      }
    }
  }
  else if (nome == "sd_del") {
    String caminho = normalizarCaminho(arg);
    if (!SD_MMC.exists(caminho)) {
      _net.responderUDP("Arquivo nao existe\n");
    } else if (SD_MMC.remove(caminho)) {
      _storage.gravarLog("[SD] Arquivo excluido com sucesso: " + caminho);
      _net.responderUDP("Ok\n");
    } else {
      _storage.gravarLog("[ERRO] Falha ao excluir: " + caminho);
      _net.responderUDP("Erro ao deletar arquivo\n");
    }
  }
  else if (nome == "sd_exists") {
    _net.responderUDP(SD_MMC.exists(normalizarCaminho(arg)) ? "Existe\n" : "Nao existe\n");
  }
  else if (nome == "sd_size") {
    String caminho = normalizarCaminho(arg);
    if (!SD_MMC.exists(caminho)) {
      _net.responderUDP("Arquivo nao existe\n");
    } else {
      File arquivo = SD_MMC.open(caminho, FILE_READ);
      if (!arquivo || arquivo.isDirectory()) {
        _net.responderUDP("Falha ao abrir arquivo\n");
      } else {
        sprintf(pMsg, "Tamanho: %u B\n", (unsigned)arquivo.size());
        arquivo.close();
        _net.responderUDP(pMsg);
      }
    }
  }
  else if (nome == "sd_clear_log") {
    if (SD_MMC.exists("/log.txt")) SD_MMC.remove("/log.txt");
    _net.responderUDP("Log limpo\n");
  }
  else {
    _storage.gravarLog("[ERRO] Comando recebido invalido ou corrompido: " + cmd);
    _net.responderUDP("Comando Invalido\n");
  }

  free(pMsg);
}

float SlaveCommandHandler::lerTemperaturaInterna() {
  static temperature_sensor_handle_t temp_sensor = NULL;
  if (temp_sensor == NULL) {
    temperature_sensor_config_t temp_cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    if (temperature_sensor_install(&temp_cfg, &temp_sensor) != ESP_OK) return 0.0;
  }
  float tsens_out;
  temperature_sensor_enable(temp_sensor);
  temperature_sensor_get_celsius(temp_sensor, &tsens_out);
  temperature_sensor_disable(temp_sensor);
  return tsens_out;
}
