#include "StorageManager.h"
#include "time.h"

StorageManager::StorageManager() : _sdcardOk(false) {}

bool StorageManager::inicializar() {
  if (!SD_MMC.begin("/sdcard", false)) { 
    Serial.println(F("[SD] Falha ao montar o Cartao SDMMC!"));
    _sdcardOk = false;
  } else {
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
      Serial.println(F("[SD] Nenhum tipo de cartao reconhecido."));
      _sdcardOk = false;
    } else {
      _sdcardOk = true;
    }
  }
  return _sdcardOk;
}

void StorageManager::gravarLog(const String& mensagem) {
  if (!_sdcardOk) return;

  struct tm timeinfo;
  char bufferCarimbo[25]; 
  
  if (getLocalTime(&timeinfo)) {
    sprintf(bufferCarimbo, "[%02d/%02d/%04d %02d:%02d:%02d] ",
            timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    sprintf(bufferCarimbo, "[Sem Hora Sinc.] ");
  }

  File arquivoLog = SD_MMC.open("/log.txt", FILE_APPEND);
  if (arquivoLog) {
    arquivoLog.print(bufferCarimbo);
    arquivoLog.println(mensagem);
    arquivoLog.close();
    
    Serial.print(F("[SD LOG] "));
    Serial.print(bufferCarimbo);
    Serial.println(mensagem);
  }
}
