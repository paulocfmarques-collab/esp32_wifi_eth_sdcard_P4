#include "TimeManager.h"
#include "time.h"

TimeManager::TimeManager(OledDisplay& oledRef) : _oled(oledRef) {}

void TimeManager::inicializarETestarNTP() {
  _oled.adicionarLinha("Sincronizando NTP...");
  
  // Substitui a linha antiga 'configTime(-10800, 0, ...)' pelo carregador dinâmico
  carregarEConfigurarHorario();

  int tentativas = 0;
  struct tm timeinfo;
  
  while (tentativas < 15) {
    if (getLocalTime(&timeinfo, 1000)) {
      _oled.adicionarLinha("NTP OK!");
      return;
    }
    _oled.adicionarLinha("Aguardando Link...");
    delay(500);
    tentativas++;
  }
  _oled.adicionarLinha("Erro Sinc. NTP");
}

String TimeManager::obterApenasHora() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return "00:00:00";
  char buffer[10];
  sprintf(buffer, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buffer);
}

String TimeManager::obterStringDataHora() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return "Sem Hora Sinc.";
  char buffer[30];
  sprintf(buffer, "%02d/%02d/%04d - %02d:%02d:%02d", 
          timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buffer);
}

void TimeManager::imprimirDataHora() {
  _oled.adicionarLinha(obterStringDataHora());
}

void TimeManager::carregarEConfigurarHorario() {
    Preferences prefs;
    prefs.begin("relogio", true); 
    long gmtOffset = prefs.getLong("gmt", -10800); 
    int daylightOffset = prefs.getInt("dst", 0);   
    prefs.end();

    configTime(gmtOffset, daylightOffset, "a.st1.ntp.br", "pool.ntp.org", "time.nist.gov");
}

void TimeManager::atualizarFuso(long novoGmtOffset) {
    Preferences prefs;
    prefs.begin("relogio", false); 
    prefs.putLong("gmt", novoGmtOffset);
    prefs.end();
    
    carregarEConfigurarHorario(); 
}

void TimeManager::atualizarDST(int novoDaylightOffset) {
    Preferences prefs;
    prefs.begin("relogio", false); 
    prefs.putInt("dst", novoDaylightOffset);
    prefs.end();
    
    carregarEConfigurarHorario(); 
}
