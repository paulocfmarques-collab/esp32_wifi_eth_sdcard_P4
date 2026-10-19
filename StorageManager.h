#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include "SD_MMC.h"
#include "FS.h"
#include <esp_timer.h>

class StorageManager
{
  private:
    bool _sdcardOk;
    uint64_t _nextSample = 0;

  public:
    struct Telemetry {
        bool mounted = false, logExists = false, logOk = false;
        uint64_t capacity = 0, total = 0, used = 0, logSize = 0, sampledAt = 0;
        uint64_t logWrites = 0, logErrors = 0, crashes = 0, crashErrors = 0;
    };
    const Telemetry& telemetry() const { return _telemetry; }
    void sampleTelemetry();
    StorageManager();
    bool inicializar();
    void gravarLog(const String& mensagem);
    bool salvarCrash(const String& relatorio, String& caminho, String& erro);

    bool isAtivo() const
    {
        return _sdcardOk;
    }
  private:
    Telemetry _telemetry;
};

#endif
