#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include "SD_MMC.h"
#include "FS.h"

class StorageManager {
private:
  bool _sdcardOk;

public:
  StorageManager();
  bool inicializar();
  void gravarLog(const String& mensagem);
  bool isAtivo() const { return _sdcardOk; }
};

#endif
