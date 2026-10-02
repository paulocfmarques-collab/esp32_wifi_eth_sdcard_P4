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
  
  float lerTemperaturaInterna();

public:
  SlaveCommandHandler(OledDisplay& o, StorageManager& s, NetworkController& n, TimeManager& t);
  void executar(String cmd);
};

#endif
