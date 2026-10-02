#ifndef NETWORK_CONTROLLER_H
#define NETWORK_CONTROLLER_H

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ETH.h>
#include <esp_netif.h>
#include "OledDisplay.h"
#include "StorageManager.h"

class NetworkController {
private:
  WebServer _server;
  WiFiUDP _udp;
  Preferences _prefs;
  OledDisplay& _oled;
  StorageManager& _storage;

  static NetworkController* _instance;
  volatile bool _dadosProntosParaSalvar;
  String _tempSSID;
  String _tempSenha;

  static void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);
  static void handleRootCallback() { if (_instance) _instance->handleRoot(); }
  static void handleSaveCallback() { if (_instance) _instance->handleSave(); }

  void handleRoot();
  void handleSave();

public:
  static bool ethConectado;

  NetworkController(OledDisplay& oledRef, StorageManager& storageRef);
  void inicializar();
  void ativarEthernetFisica(); // CORRIGIDO: Declarado oficialmente na classe
  bool conectarWifi();
  void iniciarPortal();
  void processarPortalERedes();
  void configurarPrioridadeDeRede();
  
  WiFiUDP& getUdpDriver() { return _udp; }
  void responderUDP(const String& msg);
  void zerarConfiguracoes();
};

#endif
