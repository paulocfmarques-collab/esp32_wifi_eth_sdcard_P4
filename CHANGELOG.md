# Changelog

## 1.11.0

- Inventário geral de comandos alinhado com os recursos de diagnóstico, OTA,
  proteção do OLED e cartão SD documentados no projeto.
- Add `/logs` and a bounded `/api/logs` reader for `/log.txt` on the SD card.
- Read at most the last 8 KB and select 25, 100 or 250 trailing lines.
- Add manual/automatic refresh, local search, follow mode and explicit stale/error states.
- Keep polling from writing new log entries or waking the OLED.
- Add six log-page tests and a web wake test; all 20 browser simulation checks pass.
- Add `botao_status` to diagnose the configured button pin and its electrical state.
- Clarify onboard RST versus the external GPIO 2 user button; add a web wake shortcut.

## 1.10.0

- Add an automatic dim moving-clock screensaver after 60 seconds of inactivity.
- Turn the OLED controller off after four further minutes, keeping device services running.
- Wake on ordinary commands, display events or button presses; the first short press only wakes.
- Keep transfer/OTA progress visible and preserve long-press Wi-Fi reset behavior.
- Add `protetor` controls for activation, wake, automatic mode, timeout and status.
- Keep display access on the application core and use rollover-safe inactivity timers.

## 1.9.0

- Receive serial commands on core 0 through a bounded eight-command queue.
- Run web, UDP, OLED, SD and command execution in an explicit task on core 1.
- Add `cores`, `ota_info` and serial-only `ota_password` diagnostics.
- Add authenticated browser firmware uploads and Arduino IDE OTA.
- Generate and verify a persistent per-device OTA password, disclosed only locally.
- Validate upload size, inactive OTA partition, complete writes and final image.
- Defer reboot until after success response and suspend commands during updating.
- Add six browser upload checks, bringing the interface test suite to 13 checks.

## 1.8.0

- Add bounded serial command input at 115200 baud with LF, CR and CRLF support.
- Route replies to the originating serial, UDP or HTTP channel.
- Add the web terminal with quick actions, history and redacted Wi-Fi credentials.
- Refresh dashboard cards every five seconds without replacing terminal input.
- Add pause/resume, connection failure feedback and confirmation for web mutations.
- Bound HTTP command responses to 8 KB and defer command-triggered reboots.
- Avoid the default five-second clock wait when logging without NTP sync.
- Add seven automated browser behavior checks; physical board tests remain required.

## 1.7.0

- Add circular Wi-Fi profile commands with verified persistence and redacted logs.
- Add link, channel, PSRAM, chip, heap and clock diagnostics.
- Add SD append and directory creation/removal.
- Expand help and replace the command reference with a complete verified list.
- Validate timezone and daylight-saving command arguments.
- Preserve existing command names and aliases.


## 1.6.0

- Redesign local information and five-profile Wi-Fi pages for desktop and mobile.
- Bundle styles/scripts locally; add connection badges and password visibility controls.
- Keep stored passwords out of HTML and add explicit open-network selection.
- Validate complete form submissions and verify credential persistence before reboot.
- Attempt to restore prior profiles on write failure; report restoration errors.
- Schedule restart after the HTTP success response without a blocking delay.


## 1.5.1

- Add RTT min/mean/max and sample age to network reports.
- Show network telemetry in HTTP `/info`.
- Make LED blinking and reset-button hold nonblocking.
- Show the selected graph sample age on OLED.
- Escape single quotes in SSID HTML attributes.


## 1.5.0

- Add six OLED pages, automatic rotation and manual page commands.
- Add per-interface asynchronous gateway ping, 24-result graphs, drop/offline
  counters and four recent connection events.
- Add `net_monitor`, `tela:0..5`, `tela:next` and `tela:auto`; update `help`.
- Use GPIO 2 short press for navigation and five-second hold/release for Wi-Fi reset.
- Defer network OLED events to the main loop.
- Start Ethernet independently of Wi-Fi connection success; enable UDP in AP mode.
- Load NTP configuration on every boot.
- Reject oversized UDP commands and reserve buffer space for the terminator.
- Preserve hardware pin definitions and existing command names and aliases.
