#ifndef SLAVE_COMMAND_HANDLER_H
#define SLAVE_COMMAND_HANDLER_H

#include <Arduino.h>
#include "OledDisplay.h"
#include "StorageManager.h"
#include "NetworkController.h"
#include "TimeManager.h"
#include <atomic>
#include "freertos/task.h"

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
  void executarInterno(String cmd);
  char _serialBuffer[255] = {};
  size_t _serialLength = 0;
  bool _serialOverflow = false;
  struct SerialMessage { char command[255]; bool overflow; };
  QueueHandle_t _serialQueue = nullptr;
  TaskHandle_t _serialTask = nullptr;
  std::atomic<uint32_t> _serialDropped{0};
  void lerSerial();
  static void serialTask(void* context);

public:
  SlaveCommandHandler(OledDisplay& o, StorageManager& s, NetworkController& n, TimeManager& t);
  void executar(String cmd, NetworkController::CommandChannel channel = NetworkController::CommandChannel::UDP);
  void processarSerial();
  void iniciarSerial();
  void atualizar();
};

#endif
