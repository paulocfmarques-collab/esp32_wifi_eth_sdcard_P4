void executa_comando(String cmd) {
  adicionarLinha("> " + cmd);

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
    //Serial.println(udp.remoteIP());
    //Serial.println(udp.remotePort());
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.printf("Modelo: %s\n", ESP.getChipModel());
    udp.printf("Revisao: %d\n", ESP.getChipRevision());
    udp.printf("Nucleos: %d\n", ESP.getChipCores());
    udp.printf("CPU: %d MHz\n", ESP.getCpuFreqMHz());
    udp.printf("RAM livre: %u bytes\n", ESP.getFreeHeap());
    udp.endPacket();
  }
  else if (cmd == "RAM") // Informações sobre a RAM
  {
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.printf("Heap livre: %u\n", ESP.getFreeHeap());
    udp.printf("Menor heap livre: %u\n", ESP.getMinFreeHeap());
    udp.printf("Maior bloco livre: %u\n", ESP.getMaxAllocHeap());
    udp.endPacket();
  }
  else if (cmd == "FLASH") // Informações sobre a flash
  {
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.printf("Flash total: %u\n", ESP.getFlashChipSize());
    udp.printf("Velocidade Flash: %u\n", ESP.getFlashChipSpeed());
    udp.printf("Tamanho Sketch: %u\n", ESP.getSketchSize());
    udp.printf("Espaco livre: %u\n", ESP.getFreeSketchSpace());
    udp.endPacket();
  }
  else if (cmd == "INIT") // Motivo do reset
  {
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.printf("Motivo reset: %d\n", esp_reset_reason());    
    udp.endPacket();
  }
  else if (cmd == "UPTIME") // Tempo ligado
  {
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.printf("Uptime: %lu ms\n", millis());    
    udp.endPacket();
  }
  else if (cmd == "MAC") // MAC address
  {
    responderTudo("MAC: " + WiFi.macAddress());
  }
  else if (cmd == "NET_INFO") // MAC address
  {
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.printf("IP: ");
    udp.println(WiFi.localIP());
    udp.printf("Gateway: ");
    udp.println(WiFi.gatewayIP());
    udp.printf("Mascara de rede: ");
    udp.println(WiFi.subnetMask());
    udp.printf("RSSI: %d dbm\n", WiFi.RSSI());
    udp.printf("Nome da Rede: %s\n", WiFi.SSID());
    udp.endPacket();
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

    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.printf("LED piscou %d vezes com %d ms\n", piscadas, tempo);
    udp.endPacket();
  }
  else if (cmd.startsWith("LED_BLINK")) // Comando para piscar led com tempo
  {
    int p = cmd.indexOf(':');

    if (p > 0) 
    {
      intervaloBlink = cmd.substring(p + 1).toInt();
    }

    blinkAtivo = true;

    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.printf("Blink iniciado (%lu ms)\n", intervaloBlink);
    udp.endPacket();
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
          responderTudo("--- Lendo: " + caminho + " ---\n");
          
          // Lê o arquivo linha por linha para enviar de forma estruturada
          while (arquivo.available()) {
            String linhaArq = arquivo.readStringUntil('\n');
            linhaArq += "\n";
            responderTudo(linhaArq);
          }
          responderTudo("--- Fim do Arquivo ---\n");
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
  else 
  {
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.print("Comando Invalido\n");
    udp.endPacket();
  }
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

void responderTudo(String msg) {
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
}