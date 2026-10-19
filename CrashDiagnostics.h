#pragma once
#include <Arduino.h>
#include "StorageManager.h"

class CrashDiagnostics
{
  public:
    static String lerArquivarEApagar(StorageManager& storage, const char* motivoReset);
};
