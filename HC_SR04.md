# HC-SR04 ultrasonic sensor

Firmware: **v1.25.3**. Use this guide for the conventional 5 V HC-SR04. Check the labels on your module before wiring.

## Wiring

| HC-SR04 signal | ESP32-P4 connection |
| --- | --- |
| VCC | 5 V, P6 pin 2 |
| GND | Common ground, for example P6 pin 20 |
| TRIG | GPIO3, P6 pin 19 |
| ECHO | GPIO4, P6 pin 18, through a voltage divider |

**Do not connect the 5 V ECHO signal directly to the ESP32.** Use this divider with power off:

```text
HC-SR04 ECHO --- 2.2 kΩ ---+--- GPIO4 (P6 pin 18)
                           |
                         3.3 kΩ
                           |
                          GND
```

This divider produces about 3.0 V from a nominal 5 V echo. The documented 1 kΩ / 2 kΩ alternative uses 1 kΩ between ECHO and GPIO4, and 2 kΩ between GPIO4 and ground (about 3.33 V nominal). TRIG connects directly to GPIO3. Align the sensor with the camera on the rotating assembly and share ground with the servo supply. See the [full pin map](MAPA_PINOS_E_ESQUEMA.md).

## Measurement pipeline

The firmware sends a 12 µs trigger, captures ECHO edges by interrupt, and times out after 30 ms. Triggers are separated by at least 100 ms; sampling waits at least 100 ms after a servo command. Echo widths are converted to nominal centimeters by dividing microseconds by 58. Accepted raw distances are 2–400 cm.

Three attempts form a batch. At least two valid echoes must agree within the greater of 3 cm or 10% of the smaller distance. The closest agreeing pair is averaged. A jump from a recent confirmed value needs another compatible batch. Movement resets the batch and its distance history. The recorded angle comes from the PWM command, not physical position feedback.

## Verify a reading

1. Send `servo:angulo:90` and `ultrasonic:on`.
2. Place a flat object 30–50 cm in front of the stationary sensor.
3. Query `ultrasonic:status` twice. Look for `available: true`, a plausible `distance_cm`, and an increasing `samples` count.

`errors` counts rejected batches; `rejected_jumps` identifies jump-filter rejections. `last_batch_cm` can hold a value that was not accepted. If the measurement is unavailable, JSON reports `null` rather than zero. With the servo off, distance can still be valid but the angle is unknown. Use `ultrasonic:off` to stop triggering. Measurements pause during OTA.

Distances and acquisition angles are exposed through MQTT and [Prometheus](PROMETHEUS.md). Use the [radar CSV](RADAR_TRACKING.md) to compare accepted and rejected batches against servo motion.
