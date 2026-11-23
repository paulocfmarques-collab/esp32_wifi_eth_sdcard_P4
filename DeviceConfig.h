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
// P6 physical pin 21. Servo power must come from a suitable external supply.
#define SERVO_PIN 2
#define ULTRASONIC_TRIG_PIN 3 // P6 physical pin 19.
#define ULTRASONIC_ECHO_PIN 4 // P6 physical pin 18; never connect 5 V ECHO directly.
constexpr uint32_t ULTRASONIC_INTERVAL_MS = 100;
constexpr uint32_t ULTRASONIC_SETTLE_MS = 100;
constexpr int SERVO_MIN_ANGLE = 5;
constexpr int SERVO_MAX_ANGLE = 175;
constexpr uint32_t SERVO_STEP_MS = 20;
constexpr uint32_t CAMERA_FRAME_INTERVAL_MS = 200; // Target 5 JPEG frames/second.
constexpr int SERVO_SWEEP_STEP_DEGREES = 5;
constexpr uint32_t RADAR_SWEEP_INTERVAL_MS = 400; // About 13.6 seconds across the 170-degree arc.
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
