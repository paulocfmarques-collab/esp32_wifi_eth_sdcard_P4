# Prometheus and Grafana

Firmware: **v1.27.5**. The device exposes a pull exporter; Prometheus connects to the ESP32-P4.

1. Open `/prometheus` on the device. Set the collector host reference, exporter port and scrape interval. Defaults are port **80** and interval **15 s**.
2. Download `/prometheus.yml` or add the job below. Use the **device address** in `targets`; the collector host setting is not the scrape target.
3. Open `http://DEVICE_IP:EXPORTER_PORT/metrics` to verify the endpoint, then check the target in Prometheus.

```yaml
scrape_configs:
  - job_name: esp32_p4
    scrape_interval: 15s
    metrics_path: /metrics
    static_configs:
      - targets: ['192.168.0.125:80'] # Replace with the device address and exporter port.
```

The web portal stays on port 80. The exporter can share that port or use a separate one. The interval is 5–3600 seconds. Firmware reserves port 3232 for OTA and rejects the active camera video port. Settings are persisted in Preferences. The current API is `GET/POST /api/prometheus/config`; `POST /api/prometheus` remains supported. Use the portal to save configuration.

## Metric reference

| Area | Metric examples |
| --- | --- |
| Identity and uptime | `esp_info`, `esp_firmware_version`, `esp_status_online`, `esp_uptime_seconds`, `esp_boot_count`, `esp_reboot_count`, `esp_watchdog_resets` |
| Wi-Fi and CPU | `esp_wifi_rssi_dbm`, `esp_wifi_disconnect_count`, `esp_cpu_frequency_mhz`, `esp_cpu_usage_percent`, `esp_cpu_temperature_celsius` |
| Memory | `esp_heap_free_bytes`, `esp_heap_minimum_free_bytes`, `esp_psram_free_bytes`, `esp_flash_usage_percent` |
| UDP and HTTP | `esp_udp_packets_received_total`, `esp_udp_replies_sent_total`, `esp_udp_send_errors_total`, `application_requests_total`, `application_response_time_ms` |
| SD and logging | `sdcard_mounted`, `sdcard_free_bytes`, `sdcard_log_size_bytes`, `sdcard_log_errors_total`, `sdcard_log_rotations_total`, `sdcard_log_queue_dropped_total` |
| Camera and USB | `camera_up`, `camera_frame_age_seconds`, `camera_stream_clients`, `camera_stream_errors_total`, `usb_host_up`, `usb_hid_reports_total` |
| MQTT and edge rules | `mqtt_connected`, `mqtt_publish_errors`, `esp_edge_active_alerts`, `esp_edge_rule_active` |
| Weather | `sensor_temperature_celsius`, `sensor_humidity_percent`, `esp_weather_data_age_seconds` |
| Sonar | `ultrasonic_available`, `ultrasonic_distance_cm`, `ultrasonic_servo_angle_degrees`, `ultrasonic_samples_total`, `ultrasonic_errors_total`, `ultrasonic_sample_age_seconds` |
| Legacy tracking fields | `radar_tracking_enabled`, `radar_target_locked`, `radar_target_angle_degrees`, `radar_target_distance_cm` |

See [PrometheusMetrics.cpp](PrometheusMetrics.cpp) for the full metric definitions and labels. Availability is reported separately; an unavailable distance or angle can be `NaN`, not zero. Outdoor weather comes from Open-Meteo for Porto Alegre and is distinct from the on-chip temperature. Do not treat the commanded servo angle as physical feedback.

## Example Grafana queries

```promql
esp_cpu_temperature_celsius
ultrasonic_distance_cm and on(instance, job) (ultrasonic_available == 1)
radar_target_distance_cm and on(instance, job) (radar_target_locked == 1)
increase(ultrasonic_errors_total[5m])
```

Use Celsius, centimeters, or degrees for the corresponding panels. Scrapes are snapshots; use [radar CSV recording](RADAR_TRACKING.md) for individual measurement batches and servo transitions.

Target tracking was removed in v1.26.0. Compatibility metrics radar_tracking_enabled/radar_target_locked remain 0; target distance and angle remain NaN. They no longer represent active target selection.
