# ESP32-P4 Wi-Fi / Ethernet / SD Card Monitor

[![Platform](https://img.shields.io/badge/Platform-ESP32--P4-blue)](https://www.espressif.com/)
[![Language](https://img.shields.io/badge/Language-C%2B%2B-orange)](https://isocpp.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-green)](https://www.arduino.cc/)
[![Status](https://img.shields.io/badge/Status-Active%20Development-brightgreen)](#)
[![Docs](https://img.shields.io/badge/Docs-Ready-blueviolet)](#documentation)
[![License](https://img.shields.io/badge/License-Unlicensed-lightgrey)](#license)

> A polished embedded networking and monitoring firmware for the **ESP32-P4**.

This project combines **Wi-Fi**, **Ethernet**, **SD card logging**, an **OLED dashboard**, a **local web portal**, and a **UDP command interface** into one cohesive firmware stack.

It is built for embedded systems that need:
- local observability,
- graceful fallback behavior,
- practical device management,
- and no dependency on cloud services.

---

## Why This Project Matters

Many embedded devices are easy to build but hard to operate once deployed.

They connect to a network, but they do not always explain:
- what they are doing,
- how they are connected,
- or how to recover them when something goes wrong.

This firmware solves that by giving the ESP32-P4 its own built-in operational layer:
- a compact OLED dashboard,
- a responsive local portal,
- UDP-based diagnostics,
- SD card logging,
- and intelligent Wi-Fi / Ethernet awareness.

The result is a field-ready firmware platform that behaves like a complete embedded product.

---

## Highlights

- **Dual network support** with Wi-Fi and Ethernet
- **OLED dashboard** for real-time status visibility
- **SD card logging** for local persistence and troubleshooting
- **Local web portal** for configuration and inspection
- **UDP command interface** on port `4210`
- **Network monitoring** with ping, drops, and offline tracking
- **NTP time synchronization** for accurate logs and timestamps
- **Button-based navigation** and Wi-Fi reset support
- **Fallback AP portal** when no station connection is available

---

## Project Story

This firmware was created for a practical embedded networking scenario:

a device that must stay understandable in the field.

Instead of depending on external infrastructure, the device provides its own:
- status display,
- web-based recovery path,
- command interface,
- and persistent log storage.

That makes it useful for prototypes, test benches, lab devices, and real-world embedded deployments where visibility and recoverability matter.

---

## What It Delivers

| Capability | Value |
|---|---|
| Dual network support | Uses Wi-Fi and Ethernet with priority handling and recovery logic |
| OLED dashboard | Displays time, connectivity, system data, and network history |
| SD card logging | Stores timestamped events locally for troubleshooting |
| Local web portal | Provides browser-based status and Wi-Fi configuration pages |
| UDP control plane | Exposes diagnostics and remote actions on port 4210 |
| Network monitoring | Tracks ping results, drops, timeouts, and offline duration |
| Time synchronization | Uses NTP to keep logs and display data time-aware |
| Hardware interaction | Supports button-based navigation and Wi-Fi reset |

---

## Hardware Requirements

This project is intended for an **ESP32-P4-based board** with the following peripherals:

- **ESP32-P4**
- **SSD1306 OLED display** via I2C
- **Ethernet PHY IP101**
- **MicroSD card via SD_MMC**
- **Hardware button** for navigation and Wi-Fi reset
- **LED** for status indication

### Default Pin Mapping

| Component | Pin |
|---|---:|
| OLED SDA | GPIO 7 |
| OLED SCL | GPIO 8 |
| LED | GPIO 1 |
| Button | GPIO 2 |
| Ethernet MDC | GPIO 31 |
| Ethernet MDIO | GPIO 52 |
| Ethernet Power | GPIO 51 |

See `DeviceConfig.h` for the authoritative hardware configuration.

---

## Software Requirements

You will need:

- **Arduino IDE** or another Arduino-compatible workflow
- **ESP32 board support package** with ESP32-P4 support
- Required libraries such as:
  - Adafruit SSD1306
  - Adafruit GFX
  - SD_MMC
  - WiFi / Ethernet support from the ESP32 core

The firmware uses the native ESP-IDF ICMP implementation for gateway monitoring and does **not** require an additional ping library.

---

## Quick Start

1. Clone or download the repository.
2. Open `wifi_Eth_SDCard.ino` in Arduino IDE.
3. Keep all `.h` and `.cpp` files in the same sketch folder.
4. Install the required libraries if needed.
5. Select the appropriate ESP32-P4 board configuration.
6. Compile and upload the firmware.

If no saved Wi-Fi profile can be used, the device automatically starts its local AP portal.

---

## First Boot

On startup, the firmware:

1. Initializes the OLED display.
2. Mounts the SD card.
3. Initializes the network subsystem.
4. Attempts to connect to saved Wi-Fi profiles.
5. Starts Ethernet support.
6. Synchronizes time via NTP when available.
7. Falls back to AP mode if no station connection is established.

When portal mode is active:

- **SSID:** `ESP32_P4_CONFIG`
- **URL:** `http://192.168.4.1/`

---

## Usage

The firmware is built around three primary interaction surfaces:

### OLED Dashboard
Provides immediate visibility into system state and rotates through multiple pages automatically.

### Web Portal
Use the browser portal to inspect device status and manage Wi-Fi connectivity.

### UDP Command Interface
Send commands to UDP port **4210** for diagnostics, monitoring, and remote control.

---

## OLED Display Pages

The OLED dashboard includes six pages:

1. **Clock**
   - Date and time
   - Wi-Fi and Ethernet status summary

2. **Wi-Fi**
   - SSID
   - IP address
   - Gateway
   - RSSI
   - Channel

3. **Ethernet**
   - Link status
   - IP address
   - Gateway
   - Drop count
   - Offline duration

4. **System / SD**
   - Chip information
   - Free and minimum memory
   - SD card status
   - Uptime

5. **Gateway Ping**
   - Per-interface gateway latency
   - Response history
   - Timeout visualization

6. **Network History**
   - Recent connection events

### Button Behavior

- **Short press and release**: advance to the next page
- **Press and hold for 5 seconds, then release**: clear saved Wi-Fi settings and restart

---

## Web Portal

The firmware provides a local HTTP portal with the following endpoints:

- `/`
- `/info`
- `/wifi`

The portal is intended for:
- viewing system and network status,
- configuring Wi-Fi credentials,
- and recovering the device when station mode is not available.

It is self-contained and works directly from a browser on the local network.

---

## UDP Command Interface

The firmware listens for UDP commands on port **4210**.

### Highlights

- Commands are case-insensitive
- Arguments preserve their original case
- Maximum command length is **254 bytes**
- Responses are sent back to the sender's IP and port

### Common Commands

- `help`
- `info`
- `status`
- `version`
- `build`
- `reason`
- `alive`
- `uptime`
- `net_info`
- `net_monitor`
- `wifi_status`
- `eth_status`
- `reboot`

For the full command reference, see `COMANDOS.md`.

---

## Storage and Logging

When an SD card is available, the firmware stores operational logs locally.

### Logging Behavior

- timestamped entries when time is available
- fallback label when time is not synchronized yet
- serial mirroring of key log messages for easier debugging

This makes it easier to inspect device behavior after deployment without depending on cloud telemetry.

---

## Network Monitoring

The project includes active monitoring of network health and link quality.

### It Tracks

- gateway round-trip time
- timeouts
- online/offline state
- interface-specific measurements
- connection drops
- recent network events

### Why This Matters

A timeout does not always mean the network is down. It may also indicate:
- ICMP filtering by the gateway,
- temporary congestion,
- or a transient routing issue.

This monitoring layer makes those conditions visible directly on the device.

---

## Architecture

The firmware is organized into focused modules so the system stays maintainable and easy to extend:

- **Main orchestration** — boot flow, button handling, and loop tasks
- **Network controller** — Wi-Fi, Ethernet, portal routing, and network setup
- **OLED display manager** — page rendering and dashboard updates
- **Storage manager** — SD card detection and log persistence
- **Command handler** — UDP parsing and remote command execution
- **Portal UI** — local HTML/CSS for the built-in browser portal

This separation makes it easier to evolve the project without turning the codebase into a single monolithic sketch.

---

## Project Structure

Typical project files include:

- `wifi_Eth_SDCard.ino` — main firmware entry point
- `DeviceConfig.h` — hardware and pin definitions
- `NetworkController.*` — network logic and portal handling
- `OledDisplay.*` — OLED UI pages
- `StorageManager.*` — SD card and logging support
- `SlaveCommandHandler.*` — UDP command processing
- `PortalUi.h` — local portal markup and styling

---

## Documentation

Additional project documentation included in the repository:

- `LEIA-ME.md` — original Portuguese documentation
- `COMANDOS.md` — full command reference
- `README.md` — English project overview

---

## Suggested Next Steps

To take this project even further, strong next improvements would be:

- adding real hardware photos and wiring diagrams,
- documenting ESP32-P4 board selection in more detail,
- publishing a full English command table,
- adding GitHub Actions badges for build validation,
- including a `docs/` folder with setup references,
- and adding a contribution guide for external collaborators.

---

## Contributing

Contributions are welcome.

Useful areas for contribution include:
- documentation improvements,
- clearer hardware setup notes,
- additional status pages,
- portal UX refinements,
- and expanded command documentation.

---

## License

No license file is currently included in this repository.

If you plan to distribute or reuse this project, consider adding a license such as:

- MIT
- Apache 2.0
- BSD-3-Clause

---

## Author

Built for an ESP32-P4-based embedded networking and monitoring setup.
