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
// External navigation button between GPIO 27 and GND, active LOW.
// RST resets the MCU. BOOT (GPIO 35) shares Ethernet TXD1 on this board.
#define BOTAO_RESET   27
#define AUDIO_CODEC_ADDRESS 0x18
#define AUDIO_MCLK 13
#define AUDIO_BCLK 12
#define AUDIO_LRCK 10
#define AUDIO_DOUT 9
#define AUDIO_AMP_ENABLE 53
// Same Open-Meteo location as the M5NanoC6 reference project: Porto Alegre centre.
constexpr double WEATHER_LATITUDE = -30.03283;
constexpr double WEATHER_LONGITUDE = -51.23019;

// Configurações Ethernet (ESP32-P4 DevKit com PHY IP101)
#define ETH_PHY_TYPE   ETH_PHY_IP101  
#define ETH_PHY_ADDR   1              
#define ETH_PHY_MDC    31             
#define ETH_PHY_MDIO   52             
#define ETH_PHY_POWER  51             
#define ETH_CLK_MODE   EMAC_CLK_EXT_IN 

const int UDP_PORTA = 4210;

#endif
