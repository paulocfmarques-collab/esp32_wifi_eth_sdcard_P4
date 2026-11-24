# Changelog

Firmware history for the ESP32-P4 Radar & Network Monitor. Version numbers refer to the firmware source. Historical behavior may differ from the current [setup guide](LEIA-ME.md).

## Unreleased

- Rewrite the GitHub README and English reference guides around the current firmware, including the radar CSV workflow.
- Reconcile installation, pin mappings, command examples, and telemetry references. Documentation only; the firmware version is unchanged.

## v1.27.5 — 2026-11-23

- Inspect the user's v1.27.4 ELF without rebuilding it: CSI still calls the original heap_caps_calloc, and the wrapper is absent from linked code. The helper's ldflags were ignored.
- Enable Arduino's mixed precompiled-library discovery with precompiled=true and a valid empty ESP32-P4 GNU archive. Continue compiling the helper source; provide no precompiled machine code.
- Verify interception with a small internal-memory probe before CSI startup. Report dma_fix_active and dma_internal_allocations in camera_status and the SD log. Fail at dma_allocator with an explicit installation message if interception is missing.
- Update and reinstall the helper. No firmware compilation or flashing; successful CSI recovery still requires validation on the board.

## v1.27.4 — 2026-11-23

- Confirm v1.27.3 on the running board and reproduce the same DW-GDMA internal-RAM error from its SD log. Remove the ineffective malloc-threshold workaround: explicit capability-aware calloc bypasses it.
- Add the project-provided CameraDmaFix Arduino library and installer. Wrap heap_caps_calloc at link time and add internal-memory capability only to default allocations made by the task starting CSI, outside interrupts. Preserve explicit PSRAM allocations and other tasks; do not modify the SDK.
- Install the helper in the configured workstation sketchbook, C:/Arduino. Document the extra build dependency. Firmware compilation, flashing, and camera recovery validation remain with the user.

## v1.27.3 — 2026-11-23

- Diagnose the CSI startup failure from the running board's SD log: DW-GDMA rejects a callback context allocated outside internal RAM (`user context not in internal RAM`).
- Temporarily prefer internal RAM for default allocations during CSI startup. Restore Arduino's configured allocation threshold on both success and failure, before JPEG allocation. Explicit PSRAM image allocations remain unchanged.
- This allocation policy is process-wide during the short startup call; concurrent default allocations also prefer internal RAM in that window. Hardware recovery still requires compilation and installation by the user.

## v1.27.2 — 2026-11-23

- Align camera startup with the Arduino ESP32 4.0.0 example: open/configure/start CSI before initializing the JPEG encoder and its buffers.
- Check SCCB/CSI configuration return values and report initialization stage, saved errno, and PSRAM detection in camera_status.
- Log free/largest internal DMA memory before CSI starts. Preserve existing orientation, stream routes, and cleanup on failure.
- This is a startup adjustment and diagnostic improvement; successful camera recovery requires validation on the board. No firmware compilation or flashing performed.
## v1.27.1 — 2026-11-23

- Set the continuous sweep to 5° per 200 ms: about 6.8 seconds per 170° pass, before application delays.
- Fit ultrasonic measurements into each sweep dwell with a 50 ms settling wait, 70 ms trigger interval, and two agreeing echoes. Preserve three-attempt batches and 100 ms timing for stationary measurements.
- Keep endpoint reversal, manual positioning, OTA pause, and exclusion of target tracking. Hardware timing and sensor stability remain unvalidated.
## v1.27.0 — 2026-11-23

- Target Arduino ESP32 4.0.0 / ESP-IDF 6.1.1. Replace the ESP-MQTT dependency missing from the installed P4 package with bundled official ArduinoMqttClient 0.1.8 sources.
- Run connection, subscription, publication, and keepalive operations in a dedicated core-0 worker. Keep command execution and hardware access on the application task; bound publication storage and retry connections.
- Preserve MQTT settings, topic prefixes, entity IDs, QoS 1 publications, retained discovery/state, last will, and rejection of retained control messages. Discard stale queued data when changing broker configuration.
- Update crash diagnostics for ESP-IDF 6, where the old ELF-format configuration symbol is absent. Check video, audio, USB host, and sensor API declarations against the installed package.
- Document the upgrade and third-party license. No firmware build, flash, or physical validation has been performed for this migration.
## v1.26.0 — 2026-11-23

- Remove ultrasonic target acquisition, following, and lateral probing.
- Sweep continuously through 5°–175° at 5° per 400 ms, independently of distance results or video frame delivery. Preserve fixed positioning, OTA pause, sensor filtering, sonar display, and CSV recording.
- Remove tracking controls from the portal and clear retired Home Assistant discovery entries without changing other action IDs.
- Keep legacy target telemetry and CSV fields as unavailable/false for compatibility.
- Refresh the English documentation. Firmware compilation and physical validation are left to the user.
## v1.25.3

- Add `radar_log:on`, `radar_log:off`, and `radar_log:status` for SD-backed CSV sessions.
- Record servo commands, measurement angles, accepted and rejected distance batches, and target state.
- Add `/radar.csv` for downloading the latest stopped session. Limit recording to 120 seconds.

## v1.25.2

- Complete a sweep before selecting the nearest confirmed echo and recheck it before locking.
- Hold a stable central echo without continuous side-to-side motion; probe after roughly 850 ms without a compatible central echo.

## v1.25.1

- Give target confirmation its own 1,200 ms window after the first valid batch.
- Tolerate rejected batches during confirmation and hold the confirmed direction before probing.

## v1.25.0 — 2026-11-06

- Add the persistent eighth OLED page for sonar, with four range rings, a commanded sweep line, fading echoes, and a target marker.
- Expose the page through `tela:7`, the web terminal, and MQTT discovery. Opening it does not start the servo.

## v1.24.0 — 2026-11-06

- Advance the radar by 5° per sonar measurement during search and associate samples with their commanded positions.
- Add ultrasonic target confirmation, local probing, loss recovery, and tracking controls in Home Assistant and the web terminal.

## v1.23.0 — 2026-11-06

- Add HC-SR04 support on GPIO3/GPIO4 with interrupt-based echo capture and a timeout.
- Publish distance and acquisition angle through MQTT and Prometheus; report invalid readings as unavailable.

## v1.22.0 — 2026-11-06

- Extend servo travel to 5°–175° and add position, status, and control entities in Home Assistant.

## v1.21.2 — 2026-11-06

- Raise the JPEG target to 5 fps and remove the extra MJPEG delay.
- Synchronize the servo fallback sweep with newly transmitted frames when a viewer is connected.

## v1.21.1 — 2026-11-06

- Expand the servo range to 20°–160°.

## v1.21.0 — 2026-11-06

- Add a positional servo on GPIO2 with PWM sweep, fixed angles, and web terminal shortcuts.

## v1.20.6 — 2026-11-06

- Restore the idle screensaver on information pages, leaving the camera page persistent.

## v1.20.5 — 2026-11-06

- Remove timed page rotation and retain button navigation.

## v1.20.4 — 2026-11-06

- Apply 180° orientation to JPEG snapshots and MJPEG as well as the OLED camera view.

## v1.20.3 — 2026-11-06

- Rotate the OLED camera view by 180°.

## v1.20.2 — 2026-11-06

- Add a dedicated persistent camera page with approximately one OLED frame per second.

## v1.20.1 — 2026-10-25

- Redesign the robot screensaver eyes with animated digital irises.

## v1.20.0 — 2026-10-22

- Add high-speed USB host for HID keyboards, mice, and one FAT flash drive, with a USB status page.
- Add buffered system logging, bounded SD rotation, and USB and log metrics.

## v1.19.1 — 2026-10-22

- Improve service isolation and camera stability during streaming, and expand MQTT diagnostics.

## v1.19.0 — 2026-10-22

- Add multipart MJPEG streaming, Home Assistant video setup, and a camera image on the OLED.

## v1.18.1 — 2026-10-22

- Add the 16 MiB partition layout with two 6.25 MiB OTA application slots.

## v1.18.0 — 2026-10-22

- Add OV5647 MIPI-CSI capture and JPEG snapshots.

## v1.17.0 — 2026-10-21

- Add configurable local rules, alert history, MQTT events, and Prometheus alert metrics.

## v1.16.0 — 2026-10-21

- Add native ESP-MQTT and Home Assistant discovery.

## v1.15.1 — 2026-10-19

- Add UDP listener status and receive, reply, and error counters.

## v1.15.0 — 2026-10-19

- Add cached Open-Meteo observations for Porto Alegre.

## v1.14.1 — 2026-10-19

- Improve audio diagnostics and document the codec and amplifier wiring.

## v1.14.0 — 2026-10-19

- Add external GPIO27 navigation, robot screensaver, and audio feedback.

## v1.13.2

- Export on-chip CPU temperature and report sensor failures as unavailable.

## v1.13.1

- Align Prometheus configuration with the companion project. Persist the collector reference, exporter port, and interval; add a downloadable scrape example.

## v1.13.0

- Add Prometheus metrics for identity, uptime, Wi-Fi, CPU, memory, SD, GPIO, and application activity.

## v1.12.0

- Add `crash_info` to archive and verify crash reports on SD before clearing the internal record.
- Adapt crash diagnostics to ESP32-P4 RISC-V registers and split large UDP replies.

## v1.11.0

- Add the bounded web log viewer and button diagnostics.

## v1.10.0

- Add idle OLED dimming, wake behavior, and screensaver controls.

## v1.9.0

- Split serial reception and application processing across cores.
- Add browser and Arduino IDE OTA with image and partition checks. Later firmware replaced the original per-device OTA password with a fixed credential.

## v1.8.0

- Add serial commands at 115200 baud, a web terminal, and live dashboard updates.

## v1.7.0

- Add Wi-Fi profile commands, more hardware diagnostics, SD append and directory commands, and timezone validation.

## v1.6.0

- Redesign the dashboard and five-profile Wi-Fi setup for desktop and mobile.

## v1.5.1

- Add gateway RTT statistics, sample age, and nonblocking LED and button handling.

## v1.5.0

- Add six OLED information pages, per-interface gateway ping, connection history, and manual page commands.
- Start Ethernet independently of Wi-Fi and bound oversized UDP input.

## Other historical changes

- Make the `help` command list machine-readable and add `name` for device identification.
- Add `reset_counters` for persisted diagnostics counters.
- Record board-level pin assignments and reconcile earlier command and OTA documentation.
