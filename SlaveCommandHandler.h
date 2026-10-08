#ifndef SLAVE_COMMAND_HANDLER_H
#define SLAVE_COMMAND_HANDLER_H

#include <Arduino.h>
#include "OledDisplay.h"
#include "StorageManager.h"
#include "NetworkController.h"
#include "TimeManager.h"
#include "driver/temperature_sensor.h"

class SlaveCommandHandler {
private:
  OledDisplay& _oled;
  StorageManager& _storage;
  NetworkController& _net;
  TimeManager& _time;
  
  bool _ledBlinking = false;
  uint32_t _ledStarted = 0, _ledDuration = 0, _ledChanged = 0;
  void pararLedBlink();
  float lerTemperaturaInterna();

public:
  SlaveCommandHandler(OledDisplay& o, StorageManager& s, NetworkController& n, TimeManager& t);
  void executar(String cmd);
  void atualizar();
};

#endif
