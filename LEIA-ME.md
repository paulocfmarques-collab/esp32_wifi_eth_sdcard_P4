# Setup and operations

Current firmware source: **v1.27.5**. Start with the [project overview](README.md) and [hardware guide](MAPA_PINOS_E_ESQUEMA.md).

## Build and flash

- Install the included CSI DMA helper once: run `./tools/install-camera-dma-fix.ps1 -Sketchbook 'C:/Arduino'` from PowerShell, using your Arduino sketchbook path. See [Arduino 4 migration](ARDUINO_4_MIGRATION.md). The helper's library metadata supplies a required linker flag; keep it installed as an Arduino library.
- Use the Waveshare ESP32-P4-Module-DEV-KIT target configuration and Arduino ESP32 **4.0.0**. Install Adafruit SSD1306, Adafruit GFX, and ArduinoJson 7. The Arduino core supplies networking, SD, video, and USB host components. MQTT uses bundled ArduinoMqttClient 0.1.8 over NetworkClient; keep the entire src/ directory with the sketch. No external MQTT library installation is needed.
- Open `wifi_Eth_SDCard.ino` in its folder with all source files and `partitions.csv`. Select **16 MB Flash Size** and **Custom Partition Scheme**, and **PSRAM Enabled**. The two application slots are **6,553,600 bytes (6.25 MiB) each**. PSRAM does not increase that limit.
- Install over USB when the device does not already have the matching layout. An OTA application upload does not change the partition table.

## First boot

1. Open the serial monitor at 115200 baud and confirm `version`.
2. Use `ip` and `net_monitor` to check the active connections. Ethernet does not depend on a successful Wi-Fi connection.
3. If Wi-Fi provisioning is needed, join `ESP32_P4_CONFIG` and open `http://192.168.4.1/wifi`. The portal supports up to five saved Wi-Fi profiles.
4. Open `/info` on the device. The HTTP portal stays on port 80 even when the Prometheus exporter uses another port.
5. Check `sd_status`, `audio_status`, `camera_status`, `usb_status`, and `ultrasonic:status` for the peripherals you installed.

## Video and Home Assistant

Open `/camera` for live video. The dedicated video server normally uses port 81 and can fall back to another port in 82–89. Check `camera_status` for the actual port. Use `/stream` for MJPEG and `/snapshot` for a JPEG on that port. The target rate is 5 fps; actual throughput depends on encoding and the network. Up to two simultaneous stream clients are supported.

Configure your broker at `/mqtt`. Home Assistant discovers the device's sensors and supported actions through MQTT. Use the MJPEG IP Camera integration for video, with the stream and snapshot URLs above. MQTT carries state and control messages, not video. Command payloads must be non-retained and on the allowed list. `mqtt_status` shows the base topic; the device publishes general telemetry under `/state` and radar/servo state under `/servo/state` relative to that topic.

## USB host and SD logs

- Set the board's USB jumper for the intended port: the documented **DEVICE** position routes USB-A ports 2–4 through the hub; **HOST** routes the direct port 1. These are physical routing labels. The firmware acts as a host on the selected path.
- In Arduino IDE use **Hardware CDC and JTAG** USB mode; disable USB-OTG Device CDC/MSC/DFU at boot where applicable so TinyUSB host can own the controller.
- Supported peripherals are a HID keyboard, HID mouse and one FAT16/FAT32 flash drive. HID telemetry counts reports without logging keys or typing. The drive is mounted without formatting and its root can be inspected at `/usb`.
- The onboard microSD log uses `/log.txt`, rotated at approximately 1 MiB with four archived files. Read recent lines at `/logs`; use `log_status` to inspect the queue and drops. Radar sessions use separate CSV files. Back up the SD card before destructive file commands.

## Firmware updates

Use the Arduino IDE network target `esp32-p4-node` or the browser at `/firmware`. Check `ota_info` first. Browser uploads use the `admin` account and the OTA credential reported locally by `ota_password`. Upload the **application binary** compiled for the current board and layout. After reboot, confirm the running version with `version`.

Keep the device on a trusted network. The firmware uses a fixed OTA credential and the browser upload runs over HTTP. Do not expose the portal or exporter directly to the internet.

## Troubleshooting sequence

| Symptom | Check |
| --- | --- |
| New command is not recognized | Compare `version` and `help` with the documentation. A local source edit does not update the running board. |
| Radar points in the wrong direction | Test `servo:angulo:90` and a known distance first. Check alignment and capture a [radar CSV](RADAR_TRACKING.md). |
| Video fails to open | Read `camera_status`; use the advertised video port and check the client limit. |
| Metrics cannot be scraped | Verify the device IP and exporter port in [Prometheus setup](PROMETHEUS.md). |
| USB media does not appear | Check jumper routing, USB mode and filesystem; use `usb_status` and `usb_rescan`. |
| OLED sonar page is missing | On v1.25.0 or newer, send `tela:7`. See [the OLED guide](SONAR_OLED.md). |

See [Arduino ESP32 4.0 migration](ARDUINO_4_MIGRATION.md) for backend changes, verification limits, and post-upload checks.
