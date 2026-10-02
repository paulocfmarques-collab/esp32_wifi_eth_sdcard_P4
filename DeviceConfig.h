#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <Arduino.h>

// Definições de Hardware do Display OLED
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define MAX_LINHAS    8
#define PIN_SDA       7
#define PIN_SCL       8

// Periféricos locais da Placa
#define LED_PIN       1
#define BOTAO_RESET   2

// Configurações Ethernet (ESP32-P4 DevKit com PHY IP101)
#define ETH_PHY_TYPE   ETH_PHY_IP101  
#define ETH_PHY_ADDR   1              
#define ETH_PHY_MDC    31             
#define ETH_PHY_MDIO   52             
#define ETH_PHY_POWER  51             
#define ETH_CLK_MODE   EMAC_CLK_EXT_IN 

const int UDP_PORTA = 4210;

#endif
