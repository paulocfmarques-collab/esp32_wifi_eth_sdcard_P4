# ESP32-P4 — OLED Network Monitor

Firmware v1.7.0, based on the supplied Wi-Fi / Ethernet / SD project.
Hardware pins and all existing commands are preserved. Use the original board,
ESP32 Arduino core and libraries configured for your ESP32-P4 and Wi-Fi companion.
This update does not configure the companion or change SDMMC wiring.

## OLED pages

The SSD1306 128×64 display refreshes once per second and rotates pages every
eight seconds by default. Each frame is composed in the existing display buffer
before being transferred to the OLED. This is a monochrome display.

| Command | Page |
| --- | --- |
| `tela:0` | Clock, date and interface status |
| `tela:1` | Wi-Fi SSID, IP, gateway, RSSI and signal bars |
| `tela:2` | Ethernet status, IP, gateway, disconnect count and offline time |
| `tela:3` | CPU, free/minimum heap, SD mount status and uptime |
| `tela:4` | Gateway RTT for each interface, timeout percentage and history graph |
| `tela:5` | Four most recent connection events, timestamped since boot |
| `tela:next` | Advance one page and stop automatic rotation |
| `tela:auto` | Resume rotation every eight seconds |
| `net_monitor` | Retrieve network statistics and recent events via UDP |

A command or network log temporarily displays the original console for five
seconds. File transfer progress keeps the terminal visible until completion.
Selecting a page takes effect immediately on the next display refresh.

## Physical button

GPIO 2, wired to ground with INPUT_PULLUP:

- Short press and release: advance one page, disabling automatic rotation.
- Hold for five seconds, then release: clear saved Wi-Fi settings and restart.

The `reset_wifi` command still clears Wi-Fi settings directly. The hardware EN
reset button is independent of GPIO 2. Firmware and SD files are not erased by
this settings operation.

## Measurements

The monitor uses the native ESP-IDF ICMP API, without an additional ping library.
One asynchronous ping starts approximately every five seconds. With both links
online, Wi-Fi and Ethernet alternate, giving each a sample approximately every
ten seconds. With only one online, that interface is sampled every five seconds.
Each request is bound to its interface and targets that interface's gateway.
Timeout is one second. Bar height represents RTT on an adaptive scale; crosses
represent unanswered pings. Each interface retains 24 results.

A timeout can mean that the router filters ICMP; it does not establish that Wi-Fi
or Ethernet disconnected and does not measure Internet reachability. Connection
status, drops, offline duration and four recent events are observed by the main
loop. Statistics are stored in RAM and reset on reboot. Existing blocking SD,
LED and setup operations can delay observation of connection changes.

Ethernet now starts even when no saved Wi-Fi network connects. UDP is also opened
in AP mode. Network events are queued for processing in the main loop, avoiding
OLED I2C access from the event task. Gateway ping callbacks only publish results.
The existing Wi-Fi/Ethernet route priorities are applied as interfaces appear.
NTP settings are loaded on startup so synchronization can also use Ethernet.

## UDP and installation

UDP port: **4210**. HTTP portal: `/`, `/info`, `/wifi`. Existing system, LED,
time and SD commands remain available through `help`. Commands exceeding 254
bytes are rejected before parsing; this fixes the original buffer overflow.

Extract the ZIP and open `wifi_Eth_SDCard/wifi_Eth_SDCard.ino`. Keep all headers
and source files together, including the two new `NetworkMonitor` files.

Validation performed: source structure, local includes, command preservation,
page integration and ZIP integrity. Compilation and board tests remain pending.
Recommended board checks: each page, both links together, cable disconnect,
Wi-Fi disconnect, gateway filtering ICMP, AP-only boot, SD commands and short/
long button presses.

API references: [Espressif ICMP Echo](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/icmp_echo.html)
and [ESP-NETIF interface index](https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/api-reference/network/esp_netif.html).

## Version 1.5.1

Network reports now show successful RTT minimum/mean/maximum and sample age.
Timeouts are excluded from RTT averages. The HTTP `/info` page includes the
network report. OLED latency graphs show the selected interface's sample age.
LED blinking runs through the main loop without blocking network handling;
`led_on`/`led_off` cancel blinking. Holding the GPIO 2 button no longer blocks
network handling; releasing it after five seconds performs the Wi-Fi reset.
Single quotes are escaped in configuration page HTML attributes.

## Modern local portal — 1.6.0

Responsive `/info` and `/wifi` pages use self-contained styling and scripts.
Information cards show both links, system health, saved profiles and connection
history. The five-profile editor provides show/hide controls, active profile
badges and an explicit open-network option without exposing stored passwords.

The server validates the complete submission, verifies Preferences readback and
schedules restart only after success. Failed writes trigger a verified attempt
to restore the old profiles, with a visible error and no scheduled restart.
Multi-key updates are not power-loss atomic. Existing UDP commands are retained.

## Comandos — 1.7.0

Novos comandos: `wifi_add`, `wifi_list`, `wifi_status`, `eth_status`, `channel`,
`chip_info`, `heap_min`, `psram`, `health`, `selftest`, `ntp_status`, `sd_append`,
`sd_mkdir`, `sd_rmdir`; aliases `init` e `flash_info`. `net_info` inclui gateways,
máscaras e DNS de ambas as interfaces. Credenciais Wi-Fi são ocultadas dos logs.
`help` e `COMANDOS.md` foram atualizados; consulte o Markdown para sintaxe, exemplos
e os comandos anteriores. `set_fuso` aceita somente inteiros -12..14; `dst` somente
0 ou 1. Compile e teste na placa antes de usar esta versão.
