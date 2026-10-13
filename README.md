<div align="center">

# ESP32-P4 Network Monitor

**A local-first connectivity dashboard for Wi-Fi, Ethernet, and SD-backed diagnostics.**

Bring network status, gateway monitoring, device logs, and practical controls together
on an ESP32-P4—with an OLED at the device and a web portal or UDP interface on your LAN.

[![Build](https://img.shields.io/badge/build-not%20configured-lightgrey)](#software-requirements)
[![License](https://img.shields.io/badge/license-not%20specified-lightgrey)](#license)
[![Language](https://img.shields.io/badge/language-C%2B%2B-blue)](#project-structure)
[![Platform](https://img.shields.io/badge/platform-ESP32--P4-orange)](#hardware-requirements)

</div>

> **Project status:** Firmware v1.11.0. A board-specific Arduino build is documented in
> the repository, but hardware behavior still needs validation on the exact target board.

## Overview

Embedded network devices can be difficult to diagnose when connectivity changes,
the management interface is remote, or there is no persistent record of what happened.
This firmware brings those essentials onto one ESP32-P4: it monitors Wi-Fi and Ethernet,
shows live status on a small OLED, keeps operational logs on an SD card, and offers
local browser and UDP interfaces for inspection and control.

It is intended for developers and makers building ESP32-P4 network appliances, gateways,
or lab devices who want useful on-device diagnostics without relying on a cloud service.
Ethernet starts independently of a successful Wi-Fi connection; if saved Wi-Fi profiles
cannot connect, the device can provide a local setup access point.

## Key Features

- **Wi-Fi and Ethernet:** connect using saved Wi-Fi profiles while bringing up the
  configured IP101 Ethernet PHY; view interface addresses and connection state.
- **OLED dashboard:** six pages for time, Wi-Fi, Ethernet, device/SD status, gateway
  measurements, and recent network events.
- **Local web portal:** inspect device status, manage up to five Wi-Fi profiles, use a
  browser-based command terminal, and view SD logs.
- **UDP command interface:** send diagnostics and device commands on port **4210**.
- **Gateway monitoring:** collect per-interface ICMP round-trip measurements, timeouts,
  disconnect counts, offline duration, and recent connection events.
- **Time synchronization:** configure NTP using the active network and persist the
  UTC offset and daylight-saving setting.
- **SD storage and logs:** append timestamped events to `/log.txt` when the card is
  available; review recent log lines in the portal or through commands.
- **Firmware update options:** authenticated browser upload and ArduinoOTA are available
  when OTA is enabled and configured on the device.
- **Local interaction:** use the external GPIO 2 button to navigate the display or
  clear saved Wi-Fi profiles with a long press.

## Hardware Requirements

The firmware targets an **ESP32-P4** design with compatible Wi-Fi hardware and the
peripherals below. Exact board revisions, PHY wiring, SD wiring, and flash/partition
settings are board-specific; check the board documentation before building or flashing.

| Component | Firmware configuration |
| --- | --- |
| OLED | SSD1306, 128×64, I²C address `0x3C`; SDA GPIO 7, SCL GPIO 8 |
| Ethernet | IP101 PHY; MDC GPIO 31, MDIO GPIO 52, power GPIO 51; external clock input |
| SD card | SD_MMC interface; board-compatible card wiring required |
| Status LED | GPIO 1 |
| User button | External momentary button between GPIO 2 and GND (`INPUT_PULLUP`) |

The GPIO 2 button is **not** the board's reset button. On the Waveshare ESP32-P4-WIFI6,
BOOT/GPIO 35 is shared with Ethernet TXD1 and is not used as the project button.
The firmware does not define SD_MMC pin assignments here; use the wiring and SD
configuration appropriate to your board.

## Software Requirements

- Arduino IDE or a compatible Arduino build workflow.
- An Espressif Arduino core and board definition that support the ESP32-P4.
  The repository documents a successful build with Arduino ESP32 core **3.3.12** and
  target `esp32:esp32:esp32p4`; select options that match the actual board.
- The **Adafruit SSD1306** library and its **Adafruit GFX** dependency.
- ESP32 core facilities for Wi-Fi, Ethernet, UDP, WebServer, Preferences, SD_MMC,
  ArduinoOTA, and native ESP-IDF ICMP ping. These are provided by the selected core;
  the project does not require a separate ping library.

## Installation

1. Install the ESP32-P4 board support package and the display libraries listed above.
2. Download or clone this repository and keep the `.ino`, `.h`, and `.cpp` files
   together in the sketch directory.
3. Open `wifi_Eth_SDCard.ino` in the Arduino IDE.
4. Select the ESP32-P4 board and configure flash, USB, and partition options for the
   actual hardware. For OTA, the partition table must include `otadata` and two
   application slots.
5. Connect the required peripherals and upload the firmware, initially over USB.
6. Open the serial monitor at **115200 baud**. If no saved Wi-Fi profile connects,
   join the setup network described in [Usage](#usage).

The README and source do not prescribe one universal SD-card pinout or board revision.
Confirm those details with your hardware vendor rather than assuming another board's
configuration will work.

## Usage

At startup, the firmware initializes the OLED and SD_MMC, starts Ethernet, and tries
the saved Wi-Fi profiles. When a saved network connects, the device configures its
network routes and NTP time. If no saved Wi-Fi connection is available, it starts an
access point named **`ESP32_P4_CONFIG`** at **`192.168.4.1`**. The setup access point
is created without a password; use it only in a trusted environment.

Open `http://192.168.4.1/` while connected to that access point, or use the device's
reachable address on your local network. The short press on the external GPIO 2 button
advances the OLED page. Hold it for five seconds and release to erase saved Wi-Fi
profiles and restart; this does not erase SD-card files.

### Web Portal

The local HTTP portal includes:

| Route | Purpose |
| --- | --- |
| `/` | Portal landing page |
| `/info` | Live device/network dashboard and browser command terminal |
| `/wifi` | Edit up to five saved Wi-Fi profiles |
| `/logs` | Read recent lines from the SD-card log |
| `/firmware` | Authenticated browser firmware update |

The dashboard refreshes without reloading the full page. The Wi-Fi page does not display
saved passwords. The log page offers 25, 100, or 250 lines and reads at most the final
8 KB of `/log.txt`. The browser terminal accepts the same commands as UDP and serial;
it asks for confirmation for operations that change settings or files.

The portal is intended for a trusted local network, not direct Internet exposure.
Browser OTA requires the device-specific credentials printed to the serial monitor
during setup; the password can be retrieved with `ota_password` **only over local
serial**. ArduinoOTA advertises the hostname `esp32-p4-node` on port 3232 when active.

### UDP Commands

The firmware listens for commands on UDP port **4210**. Commands can also be sent over
serial at **115200 baud** or through the `/info` browser terminal. Responses return over
the channel that received the command.

- Command names are case-insensitive; arguments preserve their case.
- The maximum command length is **254 bytes**.
- Send `help` for the complete command list; see [`COMANDOS.md`](COMANDOS.md) for
  syntax, aliases, and operational details.

Common commands:

| Command | Purpose |
| --- | --- |
| `help` | List supported commands |
| `status` | Summarize Wi-Fi, SD, time, and heap status |
| `net_info` | Show interface addresses, gateways, masks, and DNS |
| `net_monitor` | Report gateway measurements and recent network events |
| `wifi_status` / `eth_status` | Inspect a specific network interface |
| `ntp_status` | Check local clock validity and configured time servers |
| `sd_status` / `sd_list` | Inspect SD capacity or list the root directory |
| `sd_log:20` | Return recent lines from `/log.txt` |
| `tela:next` / `tela:auto` | Step through pages or resume automatic rotation |
| `reboot` | Restart the device |

For example, a UDP client can send `status` to `<device-ip>:4210`. Commands that write
files, change settings, clear Wi-Fi, or reboot take effect on the device; review the
full command reference before automating them.

### Display Pages

The monochrome SSD1306 display refreshes its clock once per second and, by default,
rotates through six pages every eight seconds. Use `tela:0` through `tela:5` to select
a page, `tela:next` to step manually, or `tela:auto` to resume rotation.

| Page | Information |
| --- | --- |
| `tela:0` | Clock, date, and interface summary |
| `tela:1` | Wi-Fi SSID, IP, gateway, RSSI, and signal bars |
| `tela:2` | Ethernet state, IP, gateway, disconnect count, and offline duration |
| `tela:3` | CPU, heap, SD mount state, and uptime |
| `tela:4` | Per-interface gateway RTT, timeout rate, and measurement graph |
| `tela:5` | Four most recent connection events since boot |

Commands and connection logs temporarily show the console view. After 60 seconds without
interaction, OLED protection dims the display and shows a moving clock; four minutes
later, the display is turned off while device services continue running. The first
button press wakes the display without changing the page.

### Network Monitoring and Time

The monitor sends an asynchronous ICMP ping to each online interface's **gateway**,
approximately every five seconds. Wi-Fi and Ethernet take turns when both are online.
Each interface retains up to 24 results; connection history and counters are held in
RAM and reset on reboot.

A gateway ping timeout may mean ICMP is filtered. It does not by itself mean the
interface disconnected, and these measurements do not test Internet reachability.
Network connection status and gateway responsiveness are separate signals.

NTP uses `a.st1.ntp.br`, `pool.ntp.org`, and `time.nist.gov`. The default UTC offset
is -03:00 with daylight-saving adjustment disabled; both settings can be changed with
the commands in [`COMANDOS.md`](COMANDOS.md). Configuration is restored from device
preferences after reboot.

### Storage & Logging

When SD_MMC mounts successfully, operational events are appended to **`/log.txt`**.
Entries use local timestamps when time is available and indicate when the clock has not
yet synchronized. Messages are also mirrored to the serial monitor. If the SD card is
unavailable, logging is skipped; SD commands and the portal log viewer require a
working card.

Use `sd_tail:/log.txt:10` or `sd_log:10` to inspect recent entries. The portal's
`/logs` page reads the latest 25, 100, or 250 lines. Other commands for listing,
reading, appending, writing, and removing files are documented in `COMANDOS.md`.

## Project Structure

| File | Responsibility |
| --- | --- |
| `wifi_Eth_SDCard.ino` | Startup, application loop, button handling, and task setup |
| `DeviceConfig.h` | Display, GPIO, Ethernet PHY, and UDP port configuration |
| `NetworkController.*` | Wi-Fi, Ethernet, local portal, UDP, and network events |
| `NetworkMonitor.*` | Gateway ping measurements and connection history |
| `OledDisplay.*` | OLED console, dashboard pages, and display protection |
| `StorageManager.*` | SD_MMC initialization and event logging |
| `TimeManager.*` | NTP setup, local time, UTC offset, and daylight-saving settings |
| `SlaveCommandHandler.*` | Serial, UDP, and web command processing |
| `FirmwareUpdater.*` | Browser-based and ArduinoOTA firmware updates |
| `PortalUi.h` | Embedded portal pages and browser-side UI |
| `COMANDOS.md` | Detailed command reference |
| `LEIA-ME.md` | Additional project notes in Portuguese |
| `CHANGELOG.md` | Release history |

## Screenshots

There are no screenshots in the repository yet. Add images under `docs/images/` and
replace these placeholders when captures from the target hardware are available:

| OLED dashboard | Web portal |
| --- | --- |
| **Placeholder:** capture the OLED status pages and save as `docs/images/oled-dashboard.jpg`. | **Placeholder:** capture `/info` in a browser and save as `docs/images/web-portal.jpg`. |

Recommended additions: the Wi-Fi profile page, the `/logs` viewer, and a photograph
showing the ESP32-P4, OLED, Ethernet, and SD-card setup. Avoid including real network
credentials or other private information in screenshots.

## License

No license file is currently present in this repository, so no license is specified.
Before redistributing or reusing this project, ask the maintainer to add a license
that states the intended permissions and conditions.
