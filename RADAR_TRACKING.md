# Continuous radar sweep and diagnostic recording

Firmware: **v1.27.0**. Target tracking has been removed. The servo stays off at boot.

## Monitor the environment

1. Verify the [HC-SR04 wiring and fixed-angle readings](HC_SR04.md).
2. Run `servo:on` to sweep the nominal **5°–175°** arc.
3. Open `tela:7` for the persistent sonar display.
4. Run `servo:off` to stop, or `servo:angulo:90` for a fixed position.

The sweep advances **5° every 400 ms**, about **13.6 seconds per endpoint-to-endpoint pass** (27.2 seconds out and back) before application delays. It reverses only at the limits. A busy ultrasonic echo can briefly delay a step until completion or the sensor timeout; no valid echo is required to continue. There is no dependency on camera frames, target confirmation, or distance values, and no burst of catch-up steps after a delay. Manual positioning remains 1° per 20 ms. OTA pauses PWM and the sweep resumes without catching up.

The interval leaves room for the HC-SR04's existing stabilization and three-echo batch. Distance data remain filtered, published and displayed, but do not decide servo direction. This is environmental scanning, not person tracking or physical angle feedback.

The retired servo:rastrear commands cannot enable tracking. Their web controls are removed, and MQTT discovery clears the old tracking buttons and target indicator while preserving other entity IDs. Legacy telemetry fields report false/null; legacy Prometheus tracking flags remain zero and target measurements remain NaN. CSV target columns are retained for compatibility and stay empty.

## Record a CSV session

```text
radar_log:on
radar_log:status
radar_log:off
```

Start the session, reproduce the behavior, then stop it. The session stops automatically after **120 seconds**. Download it at `http://DEVICE_IP/radar.csv` before restarting or starting another session. The SD file is named `radar-*.csv`; earlier files remain on the card and can be retrieved directly. Recording does not start the motor.

| CSV fields | Meaning |
| --- | --- |
| `uptime_ms`, `event` | Device uptime at capture; position or measurement row |
| `servo_angle_deg`, `servo_mode`, `servo_command_ms` | Current commanded servo state |
| `target_angle_deg`, `target_cm` | Legacy columns; empty after target tracking removal |
| `sequence`, `measurement_ms`, `measurement_angle_deg` | Batch identity and the angle associated with that measurement |
| `batch_cm`, `distance_cm` | Batch estimate before jump rejection and the accepted distance |
| `valid`, `error`, `rejected_jumps` | Acceptance, rejection reason, and cumulative jump count |

Blank cells mean the field is unavailable or does not apply. Measurements represent completed batches, not individual pulses. The current angle can differ from the measurement angle if the servo has already advanced.

For repeatable troubleshooting, record a stationary reflector at each suspect angle for about 10 seconds, then record an equivalent sweep. Comparing those sessions separates measurement variation from motion commanded by the tracking logic.
