#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include "OledDisplay.h"

class FirmwareUpdater {
public:
  void begin(WebServer& server, OledDisplay& oled);
  void handle();
  bool busy() const { return _busy || _restartAt != 0; }
  bool writingFlash() const { return _busy && !_restartAt; }
  void imprimirCredenciais() const;
  String status() const;
private:
  WebServer* _server = nullptr;
  OledDisplay* _oled = nullptr;
  String _password, _error;
  bool _enabled = false, _busy = false;
  bool _authorized = false, _ownsUpload = false, _otaStarted = false;
  size_t _expected = 0, _received = 0;
  uint32_t _restartAt = 0, _lastChunkAt = 0;
  int _lastProgress = -1;
  bool authenticate();
  void page();
  void upload();
  void complete();
  void fail(const String& error);
  void progresso(int percent);
};
