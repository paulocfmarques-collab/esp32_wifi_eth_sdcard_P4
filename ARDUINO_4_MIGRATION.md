# Arduino ESP32 4.0 migration

Firmware source: **v1.27.0**. Target: **Arduino ESP32 4.0.0 / ESP-IDF 6.1.1**, ESP32-P4.

## Build setup

1. Select ESP32 by Espressif Systems **4.0.0** in the Arduino Boards Manager.
2. Keep all sketch files, partitions.csv, and the entire src/ directory together. Arduino compiles the MQTT source files under src/ recursively.
3. Select the matching ESP32-P4 board/revision, **16 MB Flash**, **Custom Partition Scheme**, and **PSRAM Enabled**. Keep the existing board-specific CPU setting.
4. Keep Adafruit SSD1306, Adafruit GFX, and ArduinoJson 7 available. ArduinoMqttClient does not need to be installed separately: its version 0.1.8 sources are included in the sketch.
5. For USB host, keep Hardware CDC and JTAG mode and avoid conflicting TinyUSB device-at-boot options. Do not change the button's GPIO27 routing.

The application slots and NVS layout are unchanged. Upgrading the toolchain does not itself require changing the partition layout. Compile and upload the application for the actual board, then verify version and ota_info.

## MQTT backend

The installed 4.0.0 P4 SDK does not supply the mqtt_client.h ESP-MQTT dependency used by earlier firmware. The replacement uses [official ArduinoMqttClient 0.1.8](https://github.com/arduino-libraries/ArduinoMqttClient/tree/0.1.8) over the core's NetworkClient.

- Network operations run in the mqtt_io worker on core 0 with an 8 KiB stack. The main task does not wait for MQTT connect, subscribe, or publish calls.
- Seven bounded publication slots hold approximately 15.5 KiB in total. Oversized or excess publications are rejected and counted as errors rather than growing an unbounded queue.
- Reconnect attempts are spaced by 10 seconds, with 5-second connection/acknowledgment timeouts and a 30-second MQTT keepalive.
- Broker configuration is passed to the worker as a bounded snapshot. Generation tags prevent queued messages from a previous broker configuration from being sent through the replacement connection.
- Settings in Preferences, client ID, base topic, Home Assistant discovery IDs, and allowed command payloads remain compatible.
- Publications use QoS 1. Discovery, state, and availability are retained; the last will is retained offline. Retained device commands are ignored, while the Home Assistant online birth message can trigger discovery again.
- Only the worker touches the MQTT client. Received controls enter the bounded command queue and execute on the application task, not inside a network callback.

The existing counters remain. mqtt_status additionally reports backend and worker_ready. Error-type values now describe this backend: 0 = no current connection error, 1 = connection/transport failure, 2 = broker refusal. Broker return codes remain MQTT CONNACK codes; transport_error can hold the library's negative connection result. socket_errno is retained for compatibility and is not populated by this backend.

## Other APIs

The installed 4.0.0 package still exposes the ESP_Video CSI/capture interface, JPEG encoder, USBHost/HID/MSC, I²S standard-channel interface, temperature sensor, shared I²C bus handle, and FreeRTOS idle runtime APIs used here. This was checked against local headers, not by compiling or testing hardware.

The crash-dump reader now checks flash-dump availability without the former CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF symbol, which is absent in ESP-IDF 6.

## Validation after compiling

Check version, mqtt_status, camera_status, usb_status, audio_status, ultrasonic:status, and ota_info. Confirm that MQTT reports connected and worker_ready, that Home Assistant discovery restores existing entities, and that servo:on keeps sweeping during broker unavailability. Test video, SD logging, and a real OTA transfer separately.

Automated browser/partition checks do not prove firmware compilation or electrical behavior. This migration has not been compiled, flashed, or validated on the device.
