bool inicializa_sdcard()
{
  bool bRetorno = false;

  // No ESP32-P4 DevKit, os pinos do Slot 0 sao nativos do hardware.
  // Por padrao, chamamos o .begin() sem parametros para ativar o barramento de 4-bits.
  // O parametro "/sdcard" define o ponto de montagem no sistema de arquivos. 
  if (!SD_MMC.begin("/sdcard", false)) 
  { 
    Serial.println("Falha ao montar o Cartao SD!");
    Serial.println("Verifique se o cartao esta inserido e formatado em FAT32 ou exFAT.");
  }
  else
  {
    // Exibe informações do cartão montado
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) 
    {
      Serial.println("Nenhum tipo de cartao reconhecido.");
    }
    else
    {
      Serial.print("Cartao SD do tipo: ");
      if (cardType == CARD_MMC)  Serial.println("MMC");
      else if (cardType == CARD_SD)   Serial.println("SDSC");
      else if (cardType == CARD_SDHC) Serial.println("SDHC (Alta capacidade)");
      else Serial.println("Desconhecido");

      uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
      Serial.printf("Tamanho do Cartao: %llu MB\n", cardSize);
      bRetorno = true;
    }
  }

  return bRetorno;
}

void gravarLog(String mensagem) {
  // 1. Verifica se o cartão SD foi inicializado com sucesso no setup
  if (!sdcardOk) {
    Serial.println("[LOG ERRO] Tentativa de gravar log, mas o Cartão SD não está ativo.");
    return;
  }

  // 2. Obtém o horário atual do sistema
  struct tm timeinfo;
  char bufferCarimbo[25]; // Armazena "[DD/MM/AAAA HH:MM:SS] "
  
  if (getLocalTime(&timeinfo)) {
    // Formata o carimbo de data e hora de forma idêntica à exibida no OLED
    sprintf(bufferCarimbo, "[%02d/%02d/%04d %02d:%02d:%02d] ",
            timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    // Caso o NTP ainda não tenha sincronizado por algum motivo, usa um carimbo padrão
    sprintf(bufferCarimbo, "[Sem Hora Sinc.] ");
  }

  File arquivoLog = SD_MMC.open("/log.txt", FILE_APPEND);
  
  if (arquivoLog) {
    // Escreve o carimbo de hora seguido da mensagem de log
    arquivoLog.print(bufferCarimbo);
    arquivoLog.println(mensagem);
    arquivoLog.close(); // Fecha o arquivo para garantir a gravação física no hardware
    
    // Opcional: Mostra também no Monitor Serial para debug
    Serial.print("[SD LOG] ");
    Serial.print(bufferCarimbo);
    Serial.println(mensagem);
  } else {
    Serial.println("[LOG ERRO] Falha ao abrir o arquivo /log.txt para escrita.");
  }
}