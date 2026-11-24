# Hardware and pin map

Reference: Waveshare ESP32-P4-Module-DEV-KIT, Arduino ESP32 4.0.0, firmware v1.27.5. **GPIO numbers are not physical P6 pin numbers.** Confirm the board revision and pin-1 marking before wiring.

## External wiring

| Peripheral | Signal | ESP32-P4 / P6 |
| --- | --- | --- |
| SSD1306 128 × 64 | SDA / SCL | GPIO7 / GPIO8; P6 pins 3 / 5 |
| SSD1306 | Power | 3.3 V and GND; confirm module compatibility |
| Navigation button | Active-low input | GPIO27, P6 pin 38, switch to GND |
| Servo | PWM | GPIO2, P6 pin 21; separate suitable supply and common ground |
| HC-SR04 | TRIG | GPIO3, P6 pin 19 |
| HC-SR04 | ECHO | GPIO4, P6 pin 18, **through a voltage divider** |
| HC-SR04 | Power | 5 V and GND |
| Logical LED output | Digital output | GPIO1, P6 pin 22 |

Follow the [HC-SR04 wiring guide](HC_SR04.md). The ECHO output must not be connected directly to the ESP32. Power the servo from a supply sized for the motor, with a common ground. The logical LED output is not the board's power LED.

## Board resources

| Function | Pins and configuration |
| --- | --- |
| Shared I²C / SCCB | GPIO7 SDA, GPIO8 SCL; SSD1306 `0x3C`, ES8311 `0x18`, OV5647 `0x36` |
| Audio | I²S MCLK/BCLK/LRCK/DOUT on GPIO13/12/10/9; amplifier enable GPIO53; GPIO11 is wired to codec ADC output but unused by this firmware |
| Ethernet | IP101 PHY address 1; CRS_DV/RXD0/RXD1 on GPIO28/29/30, MDC 31, TXD0/TXD1 34/35, TX_EN 49, external 50 MHz REF_CLK 50, PHY reset 51, MDIO 52 |
| microSD | SDMMC slot 0, four-bit: D0–D3 GPIO39–42, CLK 43, CMD 44, active-low power GPIO45; SDK LDO channel 4 |
| ESP32-C6 radio | ESP-Hosted SDIO slot 1: D0–D3 GPIO14–17, CLK 18, CMD 19, reset 54 |
| Camera | OV5647 on the **CAMERA/CSI** connector, with dedicated MIPI data/clock pads and shared SCCB. No separate camera XCLK/reset/power-down GPIO is assigned. |
| USB | USB-A host uses the dedicated high-speed PHY. Native USB D−/D+ use GPIO24/25. Do not use the alternative full-speed routing on GPIO26/27 while the navigation button uses GPIO27. |
| Console/programming | UART0 TX/RX GPIO37/38; serial routing also depends on the IDE USB configuration |

GPIO35 is shared with BOOT and Ethernet TXD1; use the external GPIO27 switch for navigation. RESET/RST is wired to ESP_EN. GPIO36 is a strapping pin. GPIO32/33 are also routed to the I3C connector. The CAMERA/CSI and DISPLAY/DSI connectors are not interchangeable.

## P6 physical header

Identify pin 1 on your board. The reference component-side view places 3.3 V at the upper left and 5 V at the upper right.

| Odd pin | Signal | Even pin | Signal |
| ---: | --- | ---: | --- |
| 1 | 3.3 V | 2 | 5 V |
| 3 | GPIO7 / SDA | 4 | 5 V |
| 5 | GPIO8 / SCL | 6 | GND |
| 7 | GPIO23 | 8 | GPIO37 / TX |
| 9 | GND | 10 | GPIO38 / RX |
| 11 | GPIO21 | 12 | GPIO22 |
| 13 | GPIO20 | 14 | GND |
| 15 | GPIO6 | 16 | GPIO5 |
| 17 | 3.3 V | 18 | GPIO4 / ECHO input |
| 19 | GPIO3 / TRIG | 20 | GND |
| 21 | GPIO2 / servo | 22 | GPIO1 / logical LED |
| 23 | GPIO0 | 24 | GPIO36 |
| 25 | GND | 26 | GPIO32 / I3C |
| 27 | GPIO24 / USB D− | 28 | GPIO25 / USB D+ |
| 29 | GPIO33 / I3C | 30 | GND |
| 31 | GPIO26 | 32 | GPIO54 / radio reset |
| 33 | GPIO48 | 34 | GND |
| 35 | GPIO53 / amplifier enable | 36 | GPIO46 |
| 37 | GPIO47 | 38 | GPIO27 / navigation |
| 39 | GND | 40 | GPIO45 / SD power |

This is a functional wiring reference. Ethernet, radio, SDMMC, audio, and USB routing include connections already present on the board. Compare [DeviceConfig.h](DeviceConfig.h) with your board schematic and the Arduino `esp32p4` variant before reassigning pins.
