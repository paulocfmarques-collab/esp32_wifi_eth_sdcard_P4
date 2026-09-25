# ESP32 WiFi + Ethernet + SD Card + OLED Control Platform

A robust embedded project built around the ESP32-P4 DevKit that combines Wi-Fi, Ethernet, SD card logging, OLED status display, local configuration portal, and UDP command control in a single firmware platform.

This repository is organized as a set of Arduino/ESP32 `.ino` files that implement a complete device controller for monitoring and managing an ESP32-P4 system from a networked client while keeping an operation log on an SD card and showing status on an OLED display.

---

## Overview

The firmware can operate in two communication modes:

- Wi-Fi station mode, connecting to a stored SSID/password
- Wi-Fi AP mode, launching a captive configuration portal when no credentials are saved
- Ethernet fallback/parallel interface using the ESP32-P4 MAC/PHY support
- SD card logging for diagnostics and system events
- UDP-based command interface for remote control and telemetry
- OLED display for live system status
- NTP-based clock synchronization for log timestamps

This design is intended for embedded monitoring, remote diagnostics, data acquisition, and simple command-and-control applications.

---

## System Architecture

### High-level functional block diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-P4 DevKit                          │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ WiFi STA/AP  │  │ Ethernet PHY │  │ OLED Display │     │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘     │
│         │                 │                  │             │
│         └─────────────────┼──────────────────┘             │
│                           │                                │
│  ┌──────────────┐  ┌──────┴───────┐  ┌──────────────┐     │
│  │ SD Card Log  │  │ NTP Time Sync│  │ GPIO / LED   │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │         Temperature Sensor (Internal P4)             │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
        ↑                                       ↓
        │ UDP Commands (Port 4210)              │ Status/Telemetry
        │                                       │
   [Client PC]                                  [Responses]
```

### Hardware dataflow

```
Client / Host (PC / app / script)
          |
          | UDP packet (port 4210)
          v
┌─────────────────────────────┐
│ ESP32-P4 DevKit             │
│                             │
│ WiFi STA / AP              │
│ Ethernet PHY               │
│ NTP Sync                   │
│ Preferences storage        │
│ SD card logging            │
│ OLED status display        │
│ GPIO control               │
│ Temperature Sensor         │
└────────┬────────┬─────┬────┘
         │        │     │
         v        v     v
      [LED]   [OLED]  [SDCard]
              Status   Log File
```

### Pin configuration diagram

```
ESP32-P4 DevKit Pins
════════════════════════════════════════════════════════════

I2C Interface (OLED):
  GPIO7  ──────── SDA (Serial Data)
  GPIO8  ──────── SCL (Serial Clock)

LED Control:
  GPIO1  ──────── LED (Output)

Reset Button:
  GPIO2  ──────── RESET (Input, Pull-up)

Ethernet (RMII/PHY):
  GPIO31 ──────── MDC (Management Data Clock)
  GPIO52 ──────── MDIO (Management Data Input/Output)
  GPIO51 ──────── PHY Power/Reset

SD Card (native hardware):
  (Slot 0 - no external wiring needed on DevKit)

Temperature Sensor:
  (Internal - no GPIO required)
```

---

## Hardware Connections

| Function | ESP32-P4 Pin | Interface | Purpose |
| --- | --- | --- | --- |
| OLED SDA | GPIO7 | I2C | Display communication |
| OLED SCL | GPIO8 | I2C | Display clock |
| LED | GPIO1 | GPIO Output | Status indicator |
| Reset button | GPIO2 | GPIO Input (Pull-up) | Config reset trigger |
| Ethernet MDC | GPIO31 | PHY Management | Ethernet control |
| Ethernet MDIO | GPIO52 | PHY Management | Ethernet control |
| Ethernet PHY power | GPIO51 | GPIO Output | PHY reset/power |
| Network access | Network stack | WiFi / Ethernet | Internet connectivity |
| SD card | SD_MMC native | Native hardware | File storage & logging |

> **Note:** This project targets the ESP32-P4 DevKit hardware configuration. The PHY uses `ETH_PHY_IP101` by default, with `ETH_PHY_GENERIC` as fallback.

---

## Features

- Automatic Wi-Fi credential storage using `Preferences`
- Wi-Fi configuration web portal at `http://192.168.4.1`
- Synchronization with NTP servers for valid timestamps
- Ethernet initialization and route priority configuration
- SD card mount and log writing to `/log.txt`
- OLED display update with rolling message history
- UDP request/response command interface
- Temperature sensor reading for ESP32-P4
- LED control and blinking modes
- File listing and reading from SD card by remote command
- Reset button support to clear Wi-Fi settings

---

## Firmware Files

This repository includes the following `.ino` modules:

| File | Purpose | Key Functions |
| --- | --- | --- |
| `wifi_Eth_SDCard.ino` | Main application entry point | `setup()`, `loop()`, global initialization |
| `Wifi.ino` | Wi-Fi and configuration | `conectarWifi()`, `iniciarPortal()`, `salvarWifi()` |
| `eth.ino` | Ethernet management | `onNetworkEvent()`, `configurarPrioridadeDeRede()` |
| `sdcard.ino` | SD card operations | `inicializa_sdcard()`, `gravarLog()` |
| `display.ino` | OLED screen rendering | `adicionarLinha()` (scrolling text buffer) |
| `commands.ino` | UDP command processing | `executa_comando()`, `responderTudo()` |
| `utils.ino` | Utilities and NTP | `inicializarETestarNTP()`, `imprimirDataHora()` |

---

## Main Operational Logic

### Startup sequence

1. Serial console starts at `115200` baud
2. Board LED and reset button are configured
3. I2C bus and OLED display are initialized
4. SD card is mounted and logging enabled
5. Wi-Fi credentials are checked from flash storage
6. If credentials exist → connect to Wi-Fi in Station mode
7. If not → start AP mode with configuration portal
8. Ethernet interface is initialized
9. Route priorities are adjusted (WiFi preferred, Ethernet secondary)
10. NTP time synchronization is attempted
11. Device begins receiving UDP commands
12. Status is continuously logged to OLED and SD card

### Wi-Fi and configuration flow

```
Power On / Reset
     |
     v
Initialize Hardware (GPIO, I2C, SD Card)
     |
     v
Check Stored WiFi Credentials
     |
     +──── Yes ──────→ Connect to WiFi (STA mode)
     |                       |
     |                       v
     |                 Enable UDP Listener
     |                       |
     |                       v
     |                 Sync Time with NTP
     |                       |
     |                       v
     |                 Ready for Commands
     |
     +──── No ───────→ Start Access Point
                      SSID: ESP32_P4_CONFIG
                             |
                             v
                      Serve Config Portal
                      (http://192.168.4.1)
                             |
                             v
                      User enters SSID+Password
                             |
                             v
                      Save to Flash Storage
                             |
                             v
                      Restart Device
```

---

## Command Interface

The project listens on **UDP port 4210** and interprets commands from remote clients.

### Supported Commands

| Command | Description | Response |
| --- | --- | --- |
| `RESET_WIFI` | Clear saved credentials and restart | LED blinks, restarts after log message |
| `LED_ON` | Turn LED on continuously | "LED ligado" |
| `LED_OFF` | Turn LED off | "LED desligado" |
| `TEMP` | Read internal P4 temperature | Temperature in °C |
| `CPU` | Get CPU model, cores, frequency, RAM | CPU specs |
| `RAM` | Get heap memory status | Free heap, minimum, max allocable |
| `FLASH` | Get flash size and usage | Flash size, speed, sketch size |
| `INIT` | Get reset reason | Reset reason code |
| `UPTIME` | Get time since last boot | Milliseconds |
| `MAC` | Get device MAC address | MAC address string |
| `NET_INFO` | Get network details | IP, gateway, mask, RSSI, SSID |
| `LIST` | List files on SD card | File names and sizes |
| `READ:path` | Read file contents from SD | File contents |
| `DEL:path` | Delete file from SD card | Deletion status |
| `LED_PISCA:x:y` | Flash LED x times at y ms intervals | "LED piscou X vezes com Y ms" |
| `LED_BLINK:n` | Continuous LED blink at n ms interval | "Blink iniciado (N ms)" |

### Example UDP Commands

```bash
# Turn on the LED
echo "LED_ON" | nc -u 192.168.1.100 4210

# Get temperature
echo "TEMP" | nc -u 192.168.1.100 4210

# List SD card files
echo "LIST" | nc -u 192.168.1.100 4210

# Read log file
echo "READ:/log.txt" | nc -u 192.168.1.100 4210

# Flash LED 10 times at 200ms intervals
echo "LED_PISCA:10:200" | nc -u 192.168.1.100 4210

# Get system info
echo "NET_INFO" | nc -u 192.168.1.100 4210
```

---

## Configuration Portal

When no Wi-Fi credentials are stored, the ESP32 starts an access point:

```
Network Name (SSID):  ESP32_P4_CONFIG
IP Address:           192.168.4.1
No password required
```

Connect to this network and open a browser to `http://192.168.4.1` to see the configuration form:

```
┌──────────────────────────────┐
│  Configuração WiFi - ESP32-P4│
├──────────────────────────────┤
│ SSID:                        │
│ [________________________]   │
│                              │
│ Senha:                       │
│ [________________________]   │
│                              │
│          [ Salvar ]          │
└──────────────────────────────┘
```

After saving, the device stores credentials in non-volatile memory and restarts to connect automatically.

---

## SD Card Logging

The SD card is mounted at mount point `/sdcard` and logs are written to:

```
/log.txt
```

Each log entry is prefixed with a timestamp (if NTP has synced):

```
[DD/MM/YYYY HH:MM:SS] Log message here
[DD/MM/YYYY HH:MM:SS] Another event
[Sem Hora Sinc.] Message when NTP not ready
```

### Example log file

```
[25/09/2026 08:15:30] OLED Pronto!
[25/09/2026 08:15:31] Cartao SD do tipo: SDHC (Alta capacidade)
[25/09/2026 08:15:31] Tamanho do Cartao: 32 MB
[25/09/2026 08:15:32] Wifi Conectado!
[25/09/2026 08:15:32] 192.168.1.45
[25/09/2026 08:15:33] NTP Status SUCESSO! Sincronização concluída.
[25/09/2026 08:15:35] Interface Ethernet iniciada.
[25/09/2026 08:15:36] Cabo Ethernet conectado!
[25/09/2026 08:15:37] IP obtido via DHCP: 192.168.1.46
```

---

## NTP and Time Synchronization

The project synchronizes time with multiple NTP servers:

- `a.st1.ntp.br` (Brazilian time server)
- `pool.ntp.org` (Global NTP pool)
- `200.160.7.186` (Direct IP, avoids DNS issues)

**Timezone:** UTC-3 (Brasília Time / BRT)

This ensures:
- Log entries have accurate timestamps
- Events can be correlated across systems
- System clock is synchronized with network time

---

## Build and Upload

### Required Arduino Libraries

Install via Arduino IDE Library Manager or PlatformIO:

- `WiFi.h` (built-in)
- `WiFiUdp.h` (built-in)
- `WebServer.h` (built-in)
- `Preferences.h` (built-in)
- `ETH.h` (built-in)
- `Wire.h` (built-in)
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `FS.h` (built-in)
- `SD_MMC.h` (built-in)
- `time.h` (built-in)
- `esp_sntp.h` (built-in)
- `esp_netif.h` (built-in)

### Arduino IDE Configuration

1. **Board:** ESP32 P4 Dev Module (or similar P4-compatible board)
2. **Flash Size:** 4MB or higher
3. **CPU Frequency:** 240 MHz
4. **Upload Speed:** 921600 or 460800
5. **Port:** Select the correct COM/serial port
6. **Programmer:** None (use USB)

### Build Steps

1. Open `wifi_Eth_SDCard.ino` in Arduino IDE
2. Select the correct board and port
3. Click **Sketch** → **Verify** to compile
4. Click **Sketch** → **Upload** to program the board
5. Open Serial Monitor at 115200 baud to watch startup logs

---

## Important Notes

- **PHY Type:** Uses `ETH_PHY_IP101` by default. If you see "Caso apresente erro de ID, use ETH_PHY_GENERIC", change the define in `eth.ino`
- **Route Priority:** WiFi is set as the primary route (priority 50), Ethernet as secondary (priority 10). Adjust in `configurarPrioridadeDeRede()` if needed
- **Configuration Recovery:** Press the reset button (GPIO2) to clear stored WiFi settings without reflashing
- **LED Indicator:** The LED (GPIO1) shows device state and can be controlled or blinked via commands
- **Preferences Storage:** WiFi credentials are stored in ESP32 non-volatile flash using the `Preferences` API

---

## Troubleshooting

### OLED Display Not Showing

- **Check wiring:** Verify GPIO7 (SDA) and GPIO8 (SCL) connections
- **Verify address:** Default I2C address is `0x3C`
- **Confirm initialization:** Look for "OLED Pronto!" in serial output
- **Try address scan:** Run an I2C scanner sketch to find the actual address

### SD Card Not Detected

- **Format card:** Ensure SD card is formatted as FAT32 or exFAT
- **Seat firmly:** Reinsert the card and verify it's fully seated
- **Check logs:** Look for "Falha ao montar o Cartao SD!" message
- **Try different card:** Test with a known-good SD card

### Wi-Fi Won't Connect

- **Clear credentials:** Send `RESET_WIFI` command to clear saved settings
- **Verify SSID/password:** Double-check credentials entered in portal
- **Check signal:** Move closer to the router
- **Restart:** Power-cycle the device

### Ethernet Not Working

- **Verify cable:** Ensure Ethernet cable is connected
- **Check PHY type:** Confirm `ETH_PHY_IP101` is correct (or try `ETH_PHY_GENERIC`)
- **Inspect pins:** Verify GPIO31, GPIO52, GPIO51 connections
- **Check logs:** Look for Ethernet event messages

### NTP Sync Fails

- **Verify network:** Ensure device has internet access
- **Check servers:** Try pinging the NTP servers manually
- **Wait longer:** NTP can take 10+ seconds on first sync
- **Review logs:** Check serial output for NTP status messages

---

## Repository Structure

```
esp32_wifi_eth_sdcard_P4/
├── README.md                    (This file - project documentation)
├── wifi_Eth_SDCard.ino          (Main entry point, setup/loop)
├── Wifi.ino                     (WiFi connection & portal)
├── eth.ino                      (Ethernet initialization)
├── sdcard.ino                   (SD card & logging)
├── display.ino                  (OLED rendering)
├── commands.ino                 (UDP command processor)
├── utils.ino                    (NTP & utilities)
└── LICENSE                      (Optional - add license if desired)
```

---

## Summary

This is a **production-ready, modular firmware platform** for ESP32-P4-based embedded systems that combines:

- ✅ Dual-interface networking (WiFi + Ethernet)
- ✅ Captive configuration portal for easy setup
- ✅ Persistent SD card logging with timestamps
- ✅ Real-time OLED status display
- ✅ UDP command console for remote management
- ✅ Full system telemetry and diagnostics
- ✅ NTP time synchronization
- ✅ Modular, well-organized codebase

**Ideal applications:**
- Industrial IoT gateways
- Remote monitoring nodes
- Data acquisition systems
- Network test equipment
- Smart home devices
- Educational platforms
- Prototyping platforms

---

## Project Status

- ✅ Core Wi-Fi configuration and portal
- ✅ Ethernet hardware support
- ✅ SD card logging with timestamps
- ✅ OLED display with scrolling history
- ✅ UDP command console
- ✅ NTP synchronization
- ✅ Remote telemetry and diagnostics
- ✅ System reset and recovery

This firmware is stable and ready for deployment.

---

## Next Steps & Extensions

Consider adding:
- MQTT support for cloud connectivity
- JSON API for HTTP control
- Web dashboard interface
- OTA (Over-The-Air) firmware updates
- Data export routines (CSV/JSON)
- Additional sensor integrations
- Custom protocol handlers
- Watchdog timer support
- Power management modes

---

**For questions or contributions, feel free to open an issue or pull request!**
