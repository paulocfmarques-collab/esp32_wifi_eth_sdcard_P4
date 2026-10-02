#include "SlaveCommandHandler.h"
#include "SD_MMC.h"

#define FIRMWARE_VERSION "v1.4.2"

static String _ultimoComando = "Nenhum";
static uint32_t _totalComandos = 0;


SlaveCommandHandler::SlaveCommandHandler(OledDisplay& o, StorageManager& s, NetworkController& n, TimeManager& t)
  : _oled(o), _storage(s), _net(n), _time(t) {}

void SlaveCommandHandler::executar(String cmd) {
  cmd.trim();
  
  // Ignora o salvamento e contagem se for o próprio comando de consulta de histórico
  if (cmd != "LASTCMD" && cmd != "CMDCOUNT") {
    _ultimoComando = cmd;
    _totalComandos++;
  }

  _oled.adicionarLinha("> " + cmd);
  _storage.gravarLog("> " + cmd);

  char* pMsg = (char*)calloc(512, sizeof(char)); // Expandido para 512 para o bloco INFO
  if (!pMsg) return;
  
  if (cmd == "LASTCMD") {
    _net.responderUDP("Ultimo Comando: " + _ultimoComando + "\n");
  }
  else if (cmd == "CMDCOUNT") {
    sprintf(pMsg, "Total Comandos: %u\n", _totalComandos);
    _net.responderUDP(pMsg);
  }
  else if (cmd == "VERSION") {
      _net.responderUDP("Central Mestre:\nVersao Firmware: v1.4.3\n");
  }
  else if (cmd.startsWith("TAIL:")) 
  {
    if (!_storage.isAtivo()) {
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
      String dadosGerais = cmd.substring(5); dadosGerais.trim();
      int linhasDesejadas = 10; // Padrão
      String caminho = dadosGerais;

      int indiceSeparador = dadosGerais.indexOf(':');
      if (indiceSeparador != -1) {
        caminho = dadosGerais.substring(0, indiceSeparador);
        linhasDesejadas = dadosGerais.substring(indiceSeparador + 1).toInt();
        if (linhasDesejadas <= 0) linhasDesejadas = 10;
      }
      caminho.trim();
      if (!caminho.startsWith("/")) caminho = "/" + caminho;

      if (!SD_MMC.exists(caminho)) {
        _net.responderUDP("Arquivo nao existe\n");
      } else {
        File arquivo = SD_MMC.open(caminho, FILE_READ);
        if (!arquivo || arquivo.isDirectory()) {
          _net.responderUDP("Falha ao abrir arquivo\n");
        } else {
          // Estratégia rápida: Contar total de linhas do arquivo
          int totalLinhasArquivo = 0;
          while (arquivo.available()) {
            arquivo.readStringUntil('\n');
            totalLinhasArquivo++;
          }
          
          // Calcula a partir de qual linha deve começar a transmitir
          int linhaInicial = totalLinhasArquivo - linhasDesejadas;
          if (linhaInicial < 0) linhaInicial = 0;

          // Volta para o começo e pula até a linha correta
          arquivo.seek(0);
          int linhaAtual = 0;
          while (linhaAtual < linhaInicial && arquivo.available()) {
            arquivo.readStringUntil('\n');
            linhaAtual++;
          }

          // Transmite apenas o final solicitado
          _net.responderUDP("--- TAIL: " + caminho + " ---\n");
          while (arquivo.available()) {
            _net.responderUDP(arquivo.readStringUntil('\n') + "\n");
          }
          _net.responderUDP("--- Fim do Bloco ---\n");
          arquivo.close();
        }
      }
    }
  }
  else if (cmd.startsWith("WRITE:")) {
    if (!_storage.isAtivo()) {
      _storage.gravarLog("[ERRO] Comando WRITE rejeitado: Cartao SD inacessivel.");
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
      // Remove o prefixo "WRITE:"
      String dadosGerais = cmd.substring(6); 
      dadosGerais.trim();
      
      // Encontra o separador "=" entre o caminho do arquivo e o conteúdo do usuário
      int indiceSeparador = dadosGerais.indexOf('=');
      
      if (indiceSeparador == -1) {
        _net.responderUDP("Erro: Formato invalido. Use WRITE:/arquivo.txt=conteudo\n");
      } else {
        String caminho = dadosGerais.substring(0, indiceSeparador);
        String conteudo = dadosGerais.substring(indiceSeparador + 1);
        caminho.trim();
        
        if (!caminho.startsWith("/")) caminho = "/" + caminho;

        // Abre o arquivo em modo FILE_WRITE (limpa e sobrescreve se o arquivo já existir)
        File arquivo = SD_MMC.open(caminho, FILE_WRITE);
        if (!arquivo) {
          _storage.gravarLog("[ERRO] Falha critica ao criar/abrir arquivo para escrita: " + caminho);
          _net.responderUDP("Erro ao abrir arquivo\n");
        } else {
          arquivo.print(conteudo);
          arquivo.close();
          _storage.gravarLog("[SD] Arquivo gravado com sucesso: " + caminho);
          _net.responderUDP("Ok\n");
        }
      }
    }
  }
  else if (cmd.startsWith("DELETE:")) {
    if (!_storage.isAtivo()) {
      _storage.gravarLog("[ERRO] Comando DELETE rejeitado: Cartao SD inacessivel.");
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
      String caminho = cmd.substring(7);
      caminho.trim();
      if (!caminho.startsWith("/")) caminho = "/" + caminho;

      if (!SD_MMC.exists(caminho)) {
        _net.responderUDP("Arquivo nao existe\n");
      } else {
        if (SD_MMC.remove(caminho)) {
          _storage.gravarLog("[SD] Arquivo excluido com sucesso: " + caminho);
          _net.responderUDP("Ok\n");
        } else {
          _storage.gravarLog("[ERRO] Falha ao tentar excluir o arquivo: " + caminho);
          _net.responderUDP("Erro ao deletar arquivo\n");
        }
      }
    }
  }
  else if (cmd.startsWith("EXISTS:")) {
    if (!_storage.isAtivo()) {
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
      String caminho = cmd.substring(7); 
      caminho.trim();
      if (!caminho.startsWith("/")) caminho = "/" + caminho;

      if (SD_MMC.exists(caminho)) {
        _net.responderUDP("Existe\n");
      } else {
        _net.responderUDP("Nao existe\n");
      }
    }
  }
  else if (cmd.startsWith("SIZE:")) {
    if (!_storage.isAtivo()) {
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
      String caminho = cmd.substring(5); 
      caminho.trim();
      if (!caminho.startsWith("/")) caminho = "/" + caminho;

      if (!SD_MMC.exists(caminho)) {
        _net.responderUDP("Arquivo nao existe\n");
      } else {
        File arquivo = SD_MMC.open(caminho, FILE_READ);
        if (!arquivo || arquivo.isDirectory()) {
          _net.responderUDP("Falha ao abrir arquivo\n");
        } else {
          uint32_t tamanhoBytes = arquivo.size();
          arquivo.close();
          sprintf(pMsg, "Tamanho: %u B\n", tamanhoBytes);
          _net.responderUDP(pMsg);
        }
      }
    }
  }
  else if (cmd == "INFO")
  {
    struct tm timeinfo;
    char dataBuf[12] = "Sem Sinc.";
    char horaBuf[10] = "Sem Sinc.";
    if (getLocalTime(&timeinfo, 100)) {
      sprintf(dataBuf, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      sprintf(horaBuf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    String statusSD = _storage.isAtivo() ? "OK" : "FALHA";
    uint32_t heapLivre = ESP.getFreeHeap() / 1024;
    // Espaço livre aproximado do Sketch na Flash convertida para MB
    float flashLivre = (float)ESP.getFreeSketchSpace() / (1024.0 * 1024.0); 

    // Monta o cabeçalho completo dinâmico baseado no modelo solicitado
    sprintf(pMsg, 
            "===== DEVICE INFO =====\n"
            "Hostname: %s\n"
            "Firmware: %s\n"
            "Build: %s %s\n"
            "SSID: %s\n"
            "IP: %s\n"
            "MAC: %s\n"
            "RSSI: %d dBm\n"
            "Heap Livre: %u KB\n"
            "Flash Livre: %.1f MB\n"
            "SD Card: %s\n"
            "Data: %s\n"
            "Hora: %s\n"
            "Uptime: %lu ms\n"
            "=======================\n",
            ETH.getHostname(), // Ou WiFi.getHostname() dependendo de qual interface você quer printar
            "1.0.2", 
            __DATE__, __TIME__,
            WiFi.SSID().c_str(),
            (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString().c_str() : ETH.localIP().toString().c_str(),
            WiFi.macAddress().c_str(),
            WiFi.RSSI(),
            heapLivre,
            flashLivre,
            statusSD.c_str(),
            dataBuf,
            horaBuf,
            millis()
    );
    
    _net.responderUDP(pMsg);
  }
  else if (cmd == "RESET_WIFI") {
    free(pMsg);
    _net.zerarConfiguracoes();
    return;
  }
  else if (cmd == "RESTART") {
    _oled.adicionarLinha("Reiniciando...");
    _storage.gravarLog("[SISTEMA] Reinicializacao solicitada via comando RESTART.");
    _net.responderUDP("Reiniciando Escravo...\n");
    delay(1000);
    free(pMsg);
    ESP.restart();
    return;
  }
  else if (cmd == "BUILD") {
    // __DATE__ e __TIME__ pegam automaticamente o carimbo do momento da compilação
    sprintf(pMsg, "Compilacao Escravo:\nData: %s\nHora: %s\n", __DATE__, __TIME__);
    _net.responderUDP(pMsg);
  }  else if (cmd == "LED_ON") {
    digitalWrite(LED_PIN, HIGH);
    _net.responderUDP("LED ligado\n");
  }
  else if (cmd == "LED_OFF") {
    digitalWrite(LED_PIN, LOW);
    _net.responderUDP("LED desligado\n");
  }
  else if (cmd == "TEMP") {
    sprintf(pMsg, "Temp: %.2fC", lerTemperaturaInterna());
    _net.responderUDP(String(pMsg) + "\n");
  }
  else if (cmd == "CPU") {
    sprintf(pMsg, "Modelo: %s\nRevisao: %d\nFreq: %d MHz\nHeap Livre: %u B\n",
            ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getFreeHeap());
    _net.responderUDP(pMsg);
  }
  else if (cmd == "RAM") {
    sprintf(pMsg, "Heap livre: %u\nMenor heap: %u\nMaior bloco: %u\n",
            ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    _net.responderUDP(pMsg);
  }
  else if (cmd == "FLASH") {
    sprintf(pMsg, "Flash total: %u\nTamanho Sketch: %u\nEspaco livre: %u\n", 
            ESP.getFlashChipSize(), ESP.getSketchSize(), ESP.getFreeSketchSpace());
    _net.responderUDP(pMsg);
  }
  else if (cmd == "UPTIME") {
    sprintf(pMsg, "Uptime: %lu ms\n", millis());
    _net.responderUDP(pMsg);
  }
  else if (cmd == "MAC") {
    _net.responderUDP("MAC: " + WiFi.macAddress() + "\n");
  }
  // ─── COMANDO: DATA LOCAL ───
  else if (cmd == "DATE") {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 100)) {
      sprintf(pMsg, "Data:\n%02d/%02d/%04d\n", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      _net.responderUDP(pMsg);
    } else {
      // REGISTRO DE ERRO NO SD CARD
      _storage.gravarLog("[ERRO] Falha ao ler DATE: Relogio sem sincronizacao NTP.");
      _net.responderUDP("Erro: Sem Hora Sinc.\n");
    }
  }
  // ─── COMANDO: RELÓGIO LOCAL ───
  else if (cmd == "TIME") {
    String horaStr = _time.obterApenasHora();
    if (horaStr == "00:00:00") {
      // REGISTRO DE ERRO NO SD CARD
      _storage.gravarLog("[ERRO] Falha ao ler TIME: Relogio retornou valor nulo/padrao.");
    }
    _net.responderUDP("Hora:\n" + horaStr + "\n");
  }
  // ─── COMANDO: MONITORAR ESPAÇO LIVRE DO CARTÃO SD ───
  else if (cmd == "SD_INFO") {
    if (!_storage.isAtivo()) {
      // REGISTRO DE ERRO NO SD CARD
      _storage.gravarLog("[ERRO] Comando SD_INFO rejeitado: Cartao SD inacessivel.");
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
      uint64_t bytesTotais = SD_MMC.totalBytes();
      uint64_t bytesUsados = SD_MMC.usedBytes();
      uint64_t bytesLivres = bytesTotais - bytesUsados;

      uint32_t totalMB = bytesTotais / (1024 * 1024);
      uint32_t usadoMB = bytesUsados / (1024 * 1024);
      uint32_t livreMB = bytesLivres / (1024 * 1024);

      sprintf(pMsg, "--- Status SD Card ---\nTotal: %u MB\nUsado: %u MB\nLivre: %u MB\n----------------------\n", 
              totalMB, usadoMB, livreMB);
      _net.responderUDP(pMsg);
    }
  }
  else if (cmd == "LIST") {
    if (!_storage.isAtivo()) {
      // REGISTRO DE ERRO NO SD CARD
      _storage.gravarLog("[ERRO] Comando LIST rejeitado: Cartao SD falhou ou foi removido.");
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
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
  }
  else if (cmd.startsWith("READ:")) {
    if (!_storage.isAtivo()) {
      // REGISTRO DE ERRO NO SD CARD
      _storage.gravarLog("[ERRO] Comando READ falhou: Slot de memoria inativo.");
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
      String caminho = cmd.substring(5); caminho.trim();
      if (!caminho.startsWith("/")) caminho = "/" + caminho;

      if (!SD_MMC.exists(caminho)) {
        // REGISTRO DE ERRO NO SD CARD
        _storage.gravarLog("[ERRO] Tentativa de leitura falhou. Arquivo inexistente: " + caminho);
        _net.responderUDP("Arquivo nao existe\n");
      } else {
        File arquivo = SD_MMC.open(caminho, FILE_READ);
        if (!arquivo || arquivo.isDirectory()) {
          // REGISTRO DE ERRO NO SD CARD
          _storage.gravarLog("[ERRO] Falha critica do sistema de arquivos ao abrir para leitura: " + caminho);
          _net.responderUDP("Falha ao abrir arquivo\n");
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
            
            int porcentagemAtual = (tamanhoTotal > 0) ? (bytesLidos * 100 / tamanhoTotal) : 100;
            
            if (porcentagemAtual != ultimaPorcentagem) {
              ultimaPorcentagem = porcentagemAtual;
              _oled.configurarProgresso(true, porcentagemAtual);
            }
          }
          
          _net.responderUDP("--- Fim do Arquivo ---\n");
          arquivo.close();
          _oled.configurarProgresso(false, 0);
        }
      }
    }
  }
  else if (cmd == "STATUS") {
    // Verifica o status do Wi-Fi
    String statusWifi = (WiFi.status() == WL_CONNECTED) ? "OK" : "FALHA";
    
    // Verifica o status do SD Card
    String statusSD = _storage.isAtivo() ? "OK" : "FALHA";
    
    // Verifica o status do NTP checando se o ano não é o padrão Unix inicial (1970)
    struct tm timeinfo;
    String statusNTP = "FALHA";
    if (getLocalTime(&timeinfo, 100)) {
      if (timeinfo.tm_year + 1900 > 1970) {
        statusNTP = "OK";
      }
    }
    
    // Obtém o Heap Livre em Kilobytes
    uint32_t heapKB = ESP.getFreeHeap() / 1024;

    sprintf(pMsg, "ONLINE\nWiFi: %s\nSD: %s\nNTP: %s\nHeap: %u KB\n", 
            statusWifi.c_str(), statusSD.c_str(), statusNTP.c_str(), heapKB);
    _net.responderUDP(pMsg);
  }
  else if (cmd.startsWith("WRITE:")) {
    if (!_storage.isAtivo()) {
      _storage.gravarLog("[ERRO] Comando WRITE rejeitado: Cartao SD inacessivel.");
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
      // Remove o prefixo "WRITE:"
      String dadosGerais = cmd.substring(6); 
      dadosGerais.trim();
      
      // Encontra o separador ":" entre o caminho do arquivo e o conteúdo
      int indiceSeparador = dadosGerais.indexOf(':');
      
      if (indiceSeparador == -1) {
        _net.responderUDP("Erro: Formato invalido. Use WRITE:/arquivo.txt:conteudo\n");
      } else {
        String caminho = dadosGerais.substring(0, indiceSeparador);
        String conteudo = dadosGerais.substring(indiceSeparador + 1);
        caminho.trim();
        
        if (!caminho.startsWith("/")) caminho = "/" + caminho;

        // Abre o arquivo em modo FILE_WRITE (sobrescreve se o arquivo já existir)
        File arquivo = SD_MMC.open(caminho, FILE_WRITE);
        if (!arquivo) {
          _storage.gravarLog("[ERRO] Falha critica ao criar/abrir arquivo para escrita: " + caminho);
          _net.responderUDP("Erro ao abrir arquivo\n");
        } else {
          arquivo.print(conteudo);
          arquivo.close();
          _storage.gravarLog("[SD] Arquivo gravado com sucesso: " + caminho);
          _net.responderUDP("Ok\n");
        }
      }
    }
  }
  else if (cmd.startsWith("EXISTS:")) {
    if (!_storage.isAtivo()) {
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
      String caminho = cmd.substring(7); 
      caminho.trim();
      if (!caminho.startsWith("/")) caminho = "/" + caminho;

      if (SD_MMC.exists(caminho)) {
        _net.responderUDP("Existe\n");
      } else {
        _net.responderUDP("Nao existe\n");
      }
    }
  }
  else if (cmd.startsWith("SIZE:")) {
    if (!_storage.isAtivo()) {
      _net.responderUDP("Erro: SD inacessivel\n");
    } else {
      String caminho = cmd.substring(5); 
      caminho.trim();
      if (!caminho.startsWith("/")) caminho = "/" + caminho;

      if (!SD_MMC.exists(caminho)) {
        _net.responderUDP("Arquivo nao existe\n");
      } else {
        File arquivo = SD_MMC.open(caminho, FILE_READ);
        if (!arquivo || arquivo.isDirectory()) {
          _net.responderUDP("Falha ao abrir arquivo\n");
        } else {
          uint32_t tamanhoBytes = arquivo.size();
          arquivo.close();
          sprintf(pMsg, "Tamanho: %u B\n", tamanhoBytes);
          _net.responderUDP(pMsg);
        }
      }
    }
  }
  else if (cmd == "NET_INFO") {
      _net.responderUDP("NET:\nIP: " + WiFi.localIP().toString() + "\nRSSI: " + String(WiFi.RSSI()) + " dBm\nSSID: " + WiFi.SSID()+"\n");
  }
  // Exemplo de envio: "SET_TZ:-3" (Para UTC-3) ou "SET_TZ:1" (Para UTC+1)
  else if (cmd.startsWith("SET_TZ:")) {
    String valorStr = cmd.substring(7); 
    valorStr.trim();
    int fusoHoras = valorStr.toInt();
    
    // Converte horas para segundos (ex: -3 * 3600 = -10800)
    long novoGmtOffset = (long)fusoHoras * 3600;
    
    _time.atualizarFuso(novoGmtOffset);
    
    sprintf(pMsg, "Fuso alterado para UTC %d (%ld s)\n", fusoHoras, novoGmtOffset);
    _net.responderUDP(pMsg);
  }
  
  // Exemplo de envio: "SET_DST:1" (Ativar +1h) ou "SET_DST:0" (Desativar)
  else if (cmd.startsWith("SET_DST:")) {
    String valorStr = cmd.substring(8); 
    valorStr.trim();
    int dstHoras = valorStr.toInt();
    
    // Converte horas para segundos (ex: 1 * 3600 = 3600)
    int novoDaylightOffset = dstHoras * 3600;
    
    _time.atualizarDST(novoDaylightOffset);
    
    sprintf(pMsg, "Horario de verao: %dh (%d s)\n", dstHoras, novoDaylightOffset);
    _net.responderUDP(pMsg);
  }
  else {
    // REGISTRO DE ERRO NO SD CARD (Comando fantasma ou desconhecido recebido por UDP)
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
