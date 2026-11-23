# Command reference

Firmware: **v1.27.0**. Use serial at **115200 baud**, UDP port **4210**, or the terminal at `/info`. Send one command per line. The command input limit is 254 bytes; the web response is capped at 8 KiB. MQTT accepts only selected actions. Send `help` for the running firmware's list.

Command tokens are shown exactly as implemented, including Portuguese names. Replace placeholders before sending a command.

## Radar, camera, and display

| Command | Action |
| --- | --- |
| `servo:on` / `servo:off` | Start sweeping / remove servo pulses |
| `servo:angulo:90` | Move to a fixed angle; accepts 5–175 |
| `servo:status` | Show commanded angle, mode and sweep timing |
| `ultrasonic:on` / `ultrasonic:off` / `ultrasonic:status` | Enable, disable, or inspect measurements |
| `radar_log:on` / `radar_log:off` / `radar_log:status` | Start, stop, or inspect an SD CSV session |
| `camera_status` | Show camera readiness, frame age, video port, and client counts |
| `camera_oled` | Select the camera page |
| `tela:0` through `tela:7` | Select clock, Wi-Fi, Ethernet, system/SD, gateway ping, connection history, camera, or sonar |
| `tela:next` / `tela:auto` | Advance one page / resume the current page without timed rotation |
| `botao_status` | Inspect the external GPIO27 button |
| `protetor:on`, `off`, `auto`, `status`, `tempo:SECONDS` | Control the robot screensaver; prefix each argument with `protetor:` |
| `audio_status` / `audio_test` | Inspect audio or play a test tone |

[Sweep details and CSV download](RADAR_TRACKING.md) · [Sonar display](SONAR_OLED.md)

## Device, network, and maintenance

| Area | Commands |
| --- | --- |
| Identity and health | `help`, `name`, `version`, `build`, `info`, `status`, `alive`, `uptime`, `reason`, `health`, `selftest`, `lastcmd`, `cmdcount` |
| CPU and memory | `cores`, `temp`, `cpu`, `chip_info`, `ram`, `heap`, `heap_min`, `psram`, `flash`, `flash_info` |
| Network status | `net_info`, `net_monitor`, `eth_status`, `mac`, `ip`, `rssi`, `ssid`, `channel`, `wifi_status`, `wifi_list` |
| Wi-Fi profiles | `wifi_add:SSID\|PASSWORD`, `reset_wifi` |
| Service status | `weather_status`, `mqtt_status`, `edge_status`, `usb_status`, `usb_list`, `usb_rescan`, `log_status` |
| Time | `time`, `date`, `ntp_status`, `set_fuso:N`, `dst_on`, `dst_off`, `dst:0`, `dst:1` |
| LED | `led_on`, `led_off`, `led_blink[:DURATION_MS]` |
| Maintenance | `ota_info`, `ota_password` (serial only), `crash_info`, `reboot`, `reset_counters` |

The separator in the Wi-Fi command is a literal pipe, for example `wifi_add:MyNetwork|MyPassword`. Keep credentials out of shared transcripts. `crash_info` archives a crash report to SD and verifies the copy before clearing the internal record. `reset_wifi` clears saved profiles and restarts the device.

## SD commands

| Command | Action |
| --- | --- |
| `sd_status` / `sd_list` | Inspect card status / list the root |
| `sd_read:PATH` / `sd_tail:PATH[:LINES]` | Read a file or its trailing lines |
| `sd_log[:LINES]` | Read the device log |
| `sd_exists:PATH` / `sd_size:PATH` | Check a file or its size |
| `sd_write:PATH:TEXT` / `sd_append:PATH:TEXT` | Write or append text |
| `sd_del:PATH` / `sd_clear_log` | Delete a file or clear the active log |
| `sd_mkdir:PATH` / `sd_rmdir:PATH` | Create or remove a directory |

Compatibility aliases include `init`, `restart`, `list`, `read`, `write`, `delete`, `exists`, `size`, `tail`, `sd_info`, `set_tz`, and `set_dst`. For complete radar files, use the CSV download rather than the bounded web command response.
