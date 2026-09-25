void imprimirDataHora() 
{
  struct tm timeinfo;
  
  // Tenta obter o horário local configurado na pilha lwIP
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter estrutura de tempo local");
    return;
  }

  // Criamos buffers de texto estáticos para evitar lixo de memória (Garbage bytes)
  char bufferData[12]; // "DD/MM/AAAA\0"
  char bufferHora[9];  // "HH:MM:SS\0"

  // Formata os dados de maneira segura nas variáveis de texto
  sprintf(bufferData, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  sprintf(bufferHora, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  // Print de validação no Monitor Serial
  Serial.printf("%s - %s\n", bufferData, bufferHora);

  // Envia formatado corretamente para a sua função de empilhar linhas no OLED
  // (Substitua "adicionarLinha" pelo nome exato da sua função de escrita do OLED se for diferente)
  String dataHoraCompleta = String(bufferData) + " - " + String(bufferHora);
  adicionarLinha(dataHoraCompleta); 
}

void testarNTP_Status() {
  Serial.println("\n--- Iniciando Teste de Diagnóstico NTP ---");
  configTime(-10800, 0, "a.st1.ntp.br", "pool.ntp.org");

  int tentativas = 0;
  while (tentativas < 15) {
    // Lê o status real do cliente SNTP interno
    sntp_sync_status_t status = sntp_get_sync_status();
    
    switch (status) {
      case SNTP_SYNC_STATUS_RESET:
        Serial.println("[NTP Status] Inicializando ou Resetado...");
        break;
      case SNTP_SYNC_STATUS_IN_PROGRESS:
        Serial.println("[NTP Status] Buscando pacote na internet (Em progresso)...");
        break;
      case SNTP_SYNC_STATUS_COMPLETED:
        Serial.println("[NTP Status] SUCESSO! Hora sincronizada.");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          Serial.printf("Hora atual: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
        return; // Teste concluído com sucesso
    }
    
    tentativas++;
    delay(1000);
  }
  
  Serial.println("[NTP Status] FALHA: Timeout atingido. O servidor não respondeu.");
}

void inicializarETestarNTP() {
  Serial.println("--- Reiniciando Subsistema NTP para ESP32-P4 ---");
  
  // 1. Usa a API oficial prefixada com esp_ para parar o serviço se estiver ativo
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }

  // 2. Define o modo de operação como POLLING ativo
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);

  // 3. Define os servidores de hora usando a API correta
  esp_sntp_setservername(0, "a.st1.ntp.br");
  esp_sntp_setservername(1, "pool.ntp.org");
  esp_sntp_setservername(2, "200.160.7.186"); // IP Direto (Pula o DNS interno do rpc_core)

  // 4. Configura o fuso horário de Brasília (UTC-3)
  setenv("TZ", "BRT3", 1);
  tzset();

  // 5. Inicializa o serviço atualizado
  esp_sntp_init();
  Serial.println("[NTP] Serviço reinicializado via ESP_SNTP. Monitorando...");

  // 6. Laço de checagem do status da sincronização
  int tentativas = 0;
  while (tentativas < 20) {
    sntp_sync_status_t status = sntp_get_sync_status(); // Mantido (Função nativa lwIP exposta)
    
    if (status == SNTP_SYNC_STATUS_COMPLETED) {
      Serial.println("\n[NTP Status] SUCESSO! Sincronização concluída.");
      
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        Serial.printf("Hora atualizada: %02d:%02d:%02d\n", 
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      }
      Serial.println("------------------------------------------------");
      return;
    } 
    else if (status == SNTP_SYNC_STATUS_IN_PROGRESS) {
      Serial.print("[Em progresso] ");
    } 
    else {
      Serial.print("[Aguardando Link] ");
    }
    
    delay(1000);
    tentativas++;
  }

  Serial.println("[NTP Status] Erro: O barramento rpc_core não validou o tráfego de rede local.");
  Serial.println("------------------------------------------------");
}
