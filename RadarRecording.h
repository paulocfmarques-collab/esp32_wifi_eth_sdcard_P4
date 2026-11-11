#pragma once
#include <Arduino.h>

class WebServer;
class RadarRecording {
public:
    String command(String arg, bool sdReady);
    void handle(bool paused);
    void download(WebServer& server, bool sdReady);
private:
    char _path[32]{};
    char _buffer[2048]{};
    size_t _used = 0;
    bool _active = false, _failed = false;
    uint32_t _start = 0, _lastFlush = 0, _sequence = 0, _moveAt = 0, _rows = 0;
    bool flush();
    String status() const;
};
extern RadarRecording radarRecording;
