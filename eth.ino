// Definições de Hardware padrão para o ESP32-P4 DevKit (PHY IP101)
#define ETH_PHY_TYPE   ETH_PHY_IP101  // Caso apresente erro de ID, use ETH_PHY_GENERIC
#define ETH_PHY_ADDR   1              // Endereço do PHY I2C/SMI
#define ETH_PHY_MDC    31             // Pino Management Data Clock
#define ETH_PHY_MDIO   52             // Pino Management Data Input/Output
#define ETH_PHY_POWER  51             // Pino de controle de energia/Reset do PHY
#define ETH_CLK_MODE   EMAC_CLK_EXT_IN // Modo do clock RMII externo

// Função de callback para monitorar eventos de rede
void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("Interface Ethernet iniciada.");
      adicionarLinha("Interface Ethernet iniciada.");
      // Opcional: Define o nome do dispositivo na rede
      ETH.setHostname("esp32-p4-node");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("Cabo Ethernet conectado!");
      adicionarLinha("Cabo Ethernet conectado!");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("IP obtido via DHCP: ");
      Serial.println(ETH.localIP());
      adicionarLinha("IP obtido via DHCP: ");
      adicionarLinha(ETH.localIP().toString().c_str());
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("Cabo Ethernet desconectado.");
      adicionarLinha("Cabo Ethernet desconectado.");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("Interface Ethernet parada.");
      adicionarLinha("Interface Ethernet parada.");
      eth_connected = false;
      break;
    default:
      break;
  }
}

void configurarPrioridadeDeRede() {
  Serial.println("[Rede] Ajustando prioridades das interfaces para evitar erro RPC...");

  // 1. Busca os ponteiros internos de cada interface criada
  esp_netif_t* netif_wifi = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_t* netif_eth  = esp_netif_get_handle_from_ifkey("ETH_DEF");

  // 2. Define prioridades diferentes se ambas as interfaces existirem
  if (netif_wifi != NULL && netif_eth != NULL) {
    // Configura uma prioridade maior para a interface que você quer como padrão (Internet)
    // Valores maiores significam maior prioridade no lwIP do ESP-IDF
    
    // Exemplo: Se você quer usar preferencialmente o WI-FI para internet agora:
    esp_netif_set_route_prio(netif_wifi, 50);
    esp_netif_set_route_prio(netif_eth, 10);
    
    Serial.println("[Rede] Prioridades configuradas: Wi-Fi (Alta) | Ethernet (Baixa)");
  } 
  else {
    Serial.println("[Rede] Alerta: Não foi possível mapear ambas as interfaces simultaneamente.");
  }
}
