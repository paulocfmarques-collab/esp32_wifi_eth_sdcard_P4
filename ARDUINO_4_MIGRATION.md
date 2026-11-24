# Arduino ESP32 4.0 migration

Firmware source: **v1.27.5**. Target: **Arduino ESP32 4.0.0 / ESP-IDF 6.1.1**, ESP32-P4.

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

Camera startup follows the package example's CSI-before-JPEG order. If startup fails, `camera_status` reports `init_stage`, `init_errno`, `init_errno_text`, and `psram_found`. The SD log also records available internal DMA memory immediately before CSI start.

The board's v1.27.2 and v1.27.3 logs identify the specific `capture_start` / errno 22 failure: `dw_gdma_channel_register_event_callbacks: user context not in internal RAM`. CSI uses explicit `heap_caps_calloc(..., MALLOC_CAP_DEFAULT)` for its controller context. The v1.27.3 malloc-threshold workaround did not affect that explicit allocation and has been removed.

Starting in v1.27.4, the project-provided **CameraDmaFix** Arduino library supplies `--wrap=heap_caps_calloc` through `library.properties`. Its wrapper adds `MALLOC_CAP_INTERNAL` only to exactly-default capability allocations in the task currently starting CSI. Other tasks, interrupt callers, and explicit PSRAM buffers retain their original allocation capabilities. The scope ends before JPEG allocation or failure cleanup; no global malloc policy is changed. No SDK archive is modified. Hardware recovery still requires validation.

Install the helper once before compiling (replace the sketchbook path when needed):

```powershell
./tools/install-camera-dma-fix.ps1 -Sketchbook 'C:/Arduino'
```

The source is in [extras/CameraDmaFix](extras/CameraDmaFix/library.properties). On the current workstation it is installed in `C:/Arduino/libraries/CameraDmaFix`. Restart Arduino IDE after updating the helper. Keep it installed as a library, including `src/esp32p4/libCameraDmaLink.a`: copying only its source loses the required linker flag.

Inspection of the user's v1.27.4 ELF confirmed that the Arduino builder ignored the helper's original `ldflags` metadata. Helper 1.0.1 uses `precompiled=true` plus a valid empty GNU archive in the MCU folder to enable the builder's linker-flag path while still compiling the C++ source. No driver binary is supplied or modified. See the [helper build notes](extras/CameraDmaFix/README.md).

Firmware v1.27.5 probes the actual wrapped allocation path before starting CSI. `dma_fix_active` must be true; `dma_internal_allocations` counts intercepted startup calls after the probe. If interception is missing, startup stops at `dma_allocator` with an installation message instead of retrying the known-broken driver path.

After installation, check `camera_status`: `ready` should be true, `init_stage` should be `running`, and `frames` should increase. If startup still fails, preserve the SDK/Serial messages around the CAMERA event; errno alone does not identify the failing driver operation.

Check version, mqtt_status, camera_status, usb_status, audio_status, ultrasonic:status, and ota_info. Confirm that MQTT reports connected and worker_ready, that Home Assistant discovery restores existing entities, and that servo:on keeps sweeping during broker unavailability. Test video, SD logging, and a real OTA transfer separately.

Automated browser/partition checks do not prove firmware compilation or electrical behavior. This migration has not been compiled, flashed, or validated on the device.
