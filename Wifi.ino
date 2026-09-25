void zerarConfiguracoes() {
  adicionarLinha("Limpando Memoria...");
  gravarLog("Limpando Memoria...");

  prefs.begin("wifi", false);
  prefs.clear(); 
  prefs.end();
  
  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.print("WiFi zerado. Reiniciando...\n");
  udp.endPacket();

  gravarLog("WiFi zerado. Reiniciando...\n");
  
  for(int i=0; i<10; i++) {
    digitalWrite(LED, HIGH); delay(100);
    digitalWrite(LED, LOW); delay(100);
  }
  ESP.restart(); 
}

void iniciarPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_P4_CONFIG");
  
  adicionarLinha("Portal Ativo!");
  adicionarLinha("Wifi: ESP32_P4_CONFIG");
  adicionarLinha("IP: 192.168.4.1");

  gravarLog("Portal Ativo!");
  gravarLog("Wifi: ESP32_P4_CONFIG");
  gravarLog("IP: 192.168.4.1");

  server.on("/", HTTP_GET, []() {
      server.send(200, "text/html", htmlPage);
  });
  server.on("/salvar", HTTP_POST, salvarWifi);
  server.begin();
}

void salvarWifi() {
  adicionarLinha("Salvando rede...");
  gravarLog("Salvando rede...");
  String novoSSID = server.arg("ssid");
  String novaSenha = server.arg("senha");

  prefs.begin("wifi", false);
  prefs.putString("ssid", novoSSID);
  prefs.putString("senha", novaSenha);
  prefs.end();

  server.send(200, "text/html", "<h2>Configuracao salva! Reiniciando...</h2>");
  delay(2000);
  ESP.restart();
}

bool conectarWifi() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("senha", "");
  prefs.end();

  if (ssid == "") return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  adicionarLinha("Conectando a:");
  adicionarLinha(ssid);
  gravarLog("Conectando a:");
  gravarLog(ssid);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    digitalWrite(LED, !digitalRead(LED)); 
    tentativas++;
  }
  digitalWrite(LED, LOW);
  return WiFi.status() == WL_CONNECTED;
}


