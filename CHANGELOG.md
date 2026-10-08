# Changelog

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
