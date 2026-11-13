# Radar tracking and diagnostic recording

Firmware: **v1.25.3**. The servo is off and tracking is disabled at boot. The system points a camera using an ultrasonic echo in one axis; it has no visual identification, physical angle feedback, or optical autofocus.

## Search and follow

1. Verify the [HC-SR04 wiring and fixed-angle readings](HC_SR04.md).
2. Run `servo:on` to sweep the nominal **5°–175°** arc.
3. Run `servo:rastrear:on` to enable target selection. Leave it off to sweep without locking.

With sonar enabled, search advances **5°** per completed batch or after a **600 ms** wait without a candidate. In tracking mode, a candidate from **10–200 cm** is held for up to **1,200 ms** after the first valid batch. A second batch must be within **25 cm** of the first. The radar completes an endpoint-to-endpoint search, selects the nearest confirmed echo collected during the search, and checks it again before locking.

Once locked, a compatible central echo keeps the camera pointed at the target. When the central echo is lost for about **850 ms**, the servo probes to either side, initially by **±6°**, expanding to **±12°** after a missed cycle. After three missed cycles or 2.5 seconds without confirmation, it returns to searching. A nearby wall or other stable reflector can be selected instead of a person.

Without sonar, sweep pacing falls back to at most 2° per camera frame with a 200 ms minimum interval. `servo:angulo:90` takes manual control; `servo:off` removes PWM. `servo:status` reports the operating mode, timing, and target. The OLED page is `tela:7`. MQTT exposes tracking controls and the target indicator; Prometheus exposes tracking enabled, lock state, angle, and distance.

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
| `target_angle_deg`, `target_cm` | Locked target, when available |
| `sequence`, `measurement_ms`, `measurement_angle_deg` | Batch identity and the angle associated with that measurement |
| `batch_cm`, `distance_cm` | Batch estimate before jump rejection and the accepted distance |
| `valid`, `error`, `rejected_jumps` | Acceptance, rejection reason, and cumulative jump count |

Blank cells mean the field is unavailable or does not apply. Measurements represent completed batches, not individual pulses. The current angle can differ from the measurement angle if the servo has already advanced.

For repeatable troubleshooting, record a stationary reflector at each suspect angle for about 10 seconds, then record an equivalent sweep. Comparing those sessions separates measurement variation from motion commanded by the tracking logic.
