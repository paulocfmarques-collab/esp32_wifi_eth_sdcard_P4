#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include "ping/ping_sock.h"
#include <esp_netif.h>

class NetworkMonitor {
public:
 struct Link {
  bool online=false, result=false, replied=false;
  uint32_t drops=0, sent=0, lost=0, rtt=0, measuredAt=0;
  uint32_t received=0, minimum=0, maximum=0;
  uint64_t totalRtt=0;
  uint64_t offlineMs=0;
  int samples[24]; uint8_t head=0;
 } links[2]; // 0: Wi-Fi, 1: Ethernet
 String events[4];
 void update(bool wifi, bool eth);
 String report() const;
private:
 bool initialized=false;
 uint32_t tick=0, next=0, epoch[2]={0,0}, sessionEpoch=0;
 uint8_t turn=0, active=0;
 IPAddress target;
 esp_ping_handle_t session=nullptr;
 portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
 bool done=false, success=false; uint32_t elapsed=0;
 static void received(esp_ping_handle_t h, void* arg);
 static void ended(esp_ping_handle_t h, void* arg);
 void event(const String& text);
};
