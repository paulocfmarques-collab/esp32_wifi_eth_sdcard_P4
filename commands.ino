void responderTudo(String msg, bool bLog=true) {
  // 1. Envia para a Serial
  Serial.print(msg);
  
  // 2. Envia para o OLED (limpando quebras de linha para não quebrar o layout)
  String msgOled = msg;
  msgOled.replace("\n", "");
  if (msgOled.length() > 0) {
    adicionarLinha(msgOled);
  }

  // 3. Envia via UDP (Apenas se houver um cliente ativo)
  if (udp.remoteIP()) {
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.print(msg);
    udp.endPacket();
  }
  if(bLog)
  {
    gravarLog(msg);
  }
}

void executa_comando(String cmd) {
  char *pMsg = (char *) calloc(255, sizeof(char));

  adicionarLinha("> " + cmd);
  gravarLog("> " + cmd);

  if (cmd == "RESET_WIFI") {
    zerarConfiguracoes(); 
  }
  else if (cmd == "LED_ON") {
    blinkAtivo = false;
    estadoLed = true;
    digitalWrite(LED, HIGH);
    responderTudo("LED ligado\n");
  }
  else if (cmd == "LED_OFF") {
    blinkAtivo = false;
    estadoLed = false;
    digitalWrite(LED, LOW);
    responderTudo("LED desligado\n");
  }
  else if (cmd == "TEMP") {
    float t = lerTemperaturaP4();
    responderTudo("Temp: " + String(t) + "C");
  }
  else if (cmd == "CPU") // Informações sobre a CPU
  {
    sprintf(pMsg,"Modelo: %s\nRevisao: %d\nNucleos: %d\nCPU: %d MHz\nRAM livre: %u bytes\n",
                  ESP.getChipModel(),
                  ESP.getChipRevision(),
                  ESP.getChipCores(),
                  ESP.getCpuFreqMHz(),
                  ESP.getFreeHeap());
    responderTudo(pMsg);
  }
  else if (cmd == "RAM") // Informações sobre a RAM
  {
    sprintf(pMsg,"Heap livre: %u\nMenor heap livre: %u\nMaior bloco livre: %u\n",
                  ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(),
                  ESP.getMaxAllocHeap());
    responderTudo(pMsg);
  }
  else if (cmd == "FLASH") // Informações sobre a flash
  {
    sprintf(pMsg,"Flash total: %u\nVelocidade Flash: %u\nTamanho Sketch: %u\nEspaco livre: %u\n", 
                  ESP.getFlashChipSize(),
                  ESP.getFlashChipSpeed(),
                  ESP.getSketchSize(),
                  ESP.getFreeSketchSpace());
    responderTudo(pMsg);
  }
  else if (cmd == "INIT") // Motivo do reset
  {
    sprintf(pMsg,"Motivo reset: %d\n", esp_reset_reason());
    responderTudo(pMsg);
  }
  else if (cmd == "UPTIME") // Tempo ligado
  {
    sprintf(pMsg,"Uptime: %lu ms\n", millis());
    responderTudo(pMsg);
  }
  else if (cmd == "MAC") // MAC address
  {
    responderTudo("MAC: " + WiFi.macAddress());
  }
  else if (cmd == "NET_INFO") // MAC address
  {
    sprintf(pMsg, "IP: %s\nGateway: %s\nMascara de rede: %s\nRSSI: %d dbm\nNome da Rede: %s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  WiFi.subnetMask().toString().c_str(),
                  WiFi.RSSI(),
                  WiFi.SSID());
    responderTudo(pMsg);
  }  
  else if (cmd.startsWith("LED_PISCA")) // Comando para piscar o LED uma quantidade de vezes
  {
    int piscadas = 10;
    int tempo = 250;

    int p1 = cmd.indexOf(':');
    int p2 = cmd.indexOf(':', p1 + 1);

    if (p1 > 0 && p2 > 0) 
    {
      piscadas = cmd.substring(p1 + 1, p2).toInt();
      tempo = cmd.substring(p2 + 1).toInt();
    }

    for (int i = 0; i < piscadas; i++) 
    {
      digitalWrite(LED, HIGH);  delay(tempo);
      digitalWrite(LED, LOW);   delay(tempo);
    }

    sprintf(pMsg,"LED piscou %d vezes com %d ms\n", piscadas, tempo);
    responderTudo(pMsg);
  }
  else if (cmd.startsWith("LED_BLINK")) // Comando para piscar led com tempo
  {
    int p = cmd.indexOf(':');

    if (p > 0) 
    {
      intervaloBlink = cmd.substring(p + 1).toInt();
    }

    blinkAtivo = true;

    sprintf(pMsg,"Blink iniciado (%lu ms)\n", intervaloBlink);
    responderTudo(pMsg);
  }
  else if (cmd == "LIST") {
    if (!sdcardOk) {
      responderTudo("Erro: SD inacessivel\n");
    } else {
      File raiz = SD_MMC.open("/");
      File arquivo = raiz.openNextFile();
      
      if (!arquivo) {
        responderTudo("Diretorio vazio\n");
      } else {
        responderTudo("--- Arquivos SD ---\n");
        while (arquivo) {
          String infoItem = String(arquivo.name()) + " (" + String(arquivo.size()) + " B)\n";
          responderTudo(infoItem);
          arquivo = raiz.openNextFile();
        }
        responderTudo("-------------------\n");
      }
      raiz.close();
    }
  }
  else if (cmd.startsWith("READ:")) {
    if (!sdcardOk) {
      responderTudo("Erro: SD inacessivel\n");
    } else {
      String caminho = cmd.substring(5); // Extrai o nome do arquivo após "READ:"
      caminho.trim();
      
      // Garante que o caminho comece com '/' caso o usuário esqueça
      if (!caminho.startsWith("/")) {
        caminho = "/" + caminho;
      }

      if (!SD_MMC.exists(caminho)) {
        responderTudo("Arquivo nao existe: " + caminho + "\n");
      } else {
        File arquivo = SD_MMC.open(caminho, FILE_READ);
        if (!arquivo || arquivo.isDirectory()) {
          responderTudo("Falha ao abrir: " + caminho + "\n");
        } else {
          responderTudo("--- Lendo: " + caminho + " ---\n", false);
          
          // Lê o arquivo linha por linha para enviar de forma estruturada
          while (arquivo.available()) {
            String linhaArq = arquivo.readStringUntil('\n');
            linhaArq += "\n";
            responderTudo(linhaArq, false);
          }
          responderTudo("--- Fim do Arquivo ---\n", false);
          responderTudo("Comando executado com sucesso!\n");
          arquivo.close();
        }
      }
    }
  }
  else if (cmd.startsWith("DEL:")) {
    if (!sdcardOk) {
      responderTudo("Erro: SD inacessivel\n");
    } else {
      String caminho = cmd.substring(4); // Extrai o nome do arquivo após "DEL:"
      caminho.trim();

      if (!caminho.startsWith("/")) {
        caminho = "/" + caminho;
      }

      if (!SD_MMC.exists(caminho)) {
        responderTudo("Arquivo nao encontrado\n");
      } else {
        if (SD_MMC.remove(caminho)) {
          responderTudo("Deletado: " + caminho + "\n");
        } else {
          responderTudo("Erro ao deletar: " + caminho + "\n");
        }
      }
    }
  }
  else if (cmd == "TIME") // MAC address
  {
    GetDataHora();
  }
  else 
  {
    responderTudo("Comando Invalido\n");
  }
  free(pMsg);
}

float lerTemperaturaP4() {
  static temperature_sensor_handle_t temp_sensor = NULL;
  if (temp_sensor == NULL) {
    temperature_sensor_config_t temp_cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    if (temperature_sensor_install(&temp_cfg, &temp_sensor) != ESP_OK) {
      return 0.0;
    }
  }
  float tsens_out;
  temperature_sensor_enable(temp_sensor);
  temperature_sensor_get_celsius(temp_sensor, &tsens_out);
  temperature_sensor_disable(temp_sensor);
  return tsens_out;
}
