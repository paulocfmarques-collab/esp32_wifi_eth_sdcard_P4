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
#include "NetworkMonitor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define NUM_REDES 5

class NetworkController {
private:
  QueueHandle_t _networkEvents = nullptr;
  NetworkMonitor _monitor;
  WebServer _server;
  WiFiUDP _udp;
  Preferences _prefs;
  OledDisplay& _oled;
  StorageManager& _storage;

  static NetworkController* _instance;
  volatile bool _dadosProntosParaSalvar;
  bool _rotasOk;
  uint32_t _restartAt = 0;
  String _tmpS[NUM_REDES];
  String _tmpP[NUM_REDES];

  static void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);
  static void handleRootCallback() { if (_instance) _instance->handleRoot(); }
  static void handleSaveCallback() { if (_instance) _instance->handleSave(); }
  static void handleInfoCallback() { if (_instance) _instance->handleInfo(); }
  static void handleWifiCallback() { if (_instance) _instance->handleWifi(); }

  void handleRoot();
  void handleSave();
  void handleInfo();
  void handleWifi();
  void configurarRotas();
  bool tentarRede(const String& ssid, const String& senha);
  void carregarRedes(String ssids[NUM_REDES], String senhas[NUM_REDES], int& idx);

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
  const NetworkMonitor& monitor() const { return _monitor; }
  void zerarConfiguracoes();
  String listarSSIDs();
  bool adicionarRede(const String& ssid, const String& senha, String& erro);
};

#endif
