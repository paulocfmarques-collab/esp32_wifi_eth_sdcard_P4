# Changelog

## 1.15.0 — 2026-10-19

- Integra Open-Meteo de Porto Alegre, seguindo coordenadas e cadência do M5NanoC6.
- Publica temperatura e umidade externas, origem/local, idade, último HTTP e sucesso da consulta no Prometheus.
- Consulta HTTPS em tarefa própria no core 0, com cache, limite de resposta e expiração; falhas preservam somente dados ainda válidos.
- Adiciona weather_status e atalho Clima externo no terminal web. Documenta ArduinoJson 7.x e a consulta Grafana com instance/job.
- Firmware não compilado nem gravado, conforme solicitado.

## 2026-10-31 — Reset dos contadores NVS

- Adicionado `reset_counters` ao UDP, Serial e terminal web. Zera somente os
  contadores boot/reboot/watchdog no namespace `prometheus`, preservando as
  preferências de coleta.
- O terminal web pede confirmação antes de executar o comando.
- Atualizados a ajuda, o guia local e o catálogo consolidado.

## 1.14.1 — 2026-10-19

- Review the ESP32-P4-Module-DEV-KIT schematic and pinout PDFs; confirm existing ES8311/NS4150B pins without changing them.
- Reset the codec, verify format/DAC register readback, and clear mute while preserving other bits.
- Match the board example's stereo I2S framing with identical samples in both slots.
- Send about 100 ms of silence after enabling the amplifier, before each tone, and drain silence at the end.
- Complete partial I2S writes with bounded waits; retain the ability to retry after a playback timeout.
- Add audio_test (500 ms / 880 Hz), a web shortcut, and extended audio_status counters/registers.
- Update all five Markdown guides. All 35 interface tests pass.
- Root cause and audible playback remain unconfirmed on hardware; no compilation or flashing performed by the assistant.


## Revisão documental — 2026-10-19

- Conferidos os cinco Markdown do projeto com o estado 1.14.0, sem alterar código ou versão.
- Alinhados botão GPIO27/GND, protetor de robôs, áudio ES8311 e comandos de diagnóstico.
- Atualizadas a descrição das tarefas e a contagem vigente de 34 testes de interface já executados.
- Acrescentados exemplos de verificação e referências entre os guias; distinguidas as notas históricas do comportamento atual.
- Nenhuma compilação, gravação ou teste físico novo nesta revisão.

## 1.14.0 — 2026-10-19

- Move the external navigation button to GPIO27/GND with pull-up and 50 ms debounce.
- Wake and advance once on press; remove the long-press Wi-Fi erase action.
- Restore a full-screen 128×64 animated robot screensaver after 60 s of user inactivity, without OLED sleep.
- Keep background logs and web/Prometheus polling from waking or resetting the saver; give OTA progress priority.
- Add protetor:on/off/auto/status/tempo commands and a robot web shortcut.
- Play an asynchronous 80 ms navigation tone through ES8311 at 0x18, I2S 13/12/10/9, amplifier GPIO53.
- Share the OLED I2C bus, keep I2C out of the audio worker, and add audio_status diagnostics.
- Update current button/OLED/audio guides and the GPIO metric pin. All 34 interface tests pass.
- Firmware compilation and physical audio/wake/OTA checks remain with the user as requested.

## 1.13.2

- Export `esp_cpu_temperature_celsius` for CPU temperature panels in Grafana.
- Share one checked sensor driver between the `temp` command and Prometheus.
- Report NaN/unavailable on sensor failures and expose CPU temperature availability.
- Document the Grafana query and Celsius unit; preserve all existing metrics and connection settings.
- No firmware compilation or physical test in this revision, as requested by the user.

## Revisão documental — 2026-10-18

- Catálogo geral corrigido para o estado 1.13.1: `protetor:*` removido,
  `sd_list` somente na raiz e ausência de `sd_test`.
- Diferenciados limite de entrada de 254 bytes, `led_blink` por duração,
  padrão de métricas na porta 80 e namespace `prometheus`.
- Preservado o histórico de OTA e as versões numeradas abaixo, sem criar
  uma nova versão por causa da documentação.
- [Histórico consolidado](../../ESP32_Documents/changelog.md).
  Nenhuma compilação ou teste físico novo nesta revisão.

## 1.13.1

- Align the connection pattern with M5NanoC6: collector host, exporter port and scrape interval saved in Preferences.
- Keep the web and configuration API on port 80; allow metrics to share port 80 or use a separate port.
- Apply exporter port changes about one second after saving, without reboot or redirect.
- Add GET/POST /api/prometheus/config and downloadable /prometheus.yml with the configured interval.
- Remove the separate server-port field and enable switch; preserve legacy host/port and default the interval to 15 seconds.
- Keep all existing metrics, including SD; retain the P4 OTA reservation on TCP port 3232.
- Validate preference loading, port sharing, interval limits and device-target links in 32 passing interface tests.
- Update all project Markdown guides. Compilation is left to the user as requested; hardware tests remain pending.

## Unreleased

- Incluído `name` na linha `help` para identificar o dispositivo como `ESP32P4`;
  corrigido o separador para não unir `name` a `ota_password`.
- Use the fixed default OTA password `123456` for Arduino IDE and browser uploads (`admin`).
- Remove per-device OTA password generation and Preferences access; old stored passwords are ignored.
- Keep authentication enabled; restrict use of this weak default password to trusted networks.

## 1.13.0

- Add a separate, configurable HTTP Prometheus exporter at `/metrics`; keep the device web UI on port 80.
- Add `/prometheus` settings and protected `/api/prometheus` POST, with browser/server validation and verified Preferences storage.
- Store collector hostname/IPv4 and server port as navigation references for pull collection; show a target and `prometheus.yml` example.
- Apply exporter port/enabled changes without restarting; reserve device web, OTA and command ports.
- Export requested identity, boot/reset, Wi-Fi, CPU, tasks, heap/PSRAM, flash, existing GPIO and HTTP application metrics.
- Persist boot/reboot/watchdog counts; count Wi-Fi disconnect events safely across tasks.
- Expose unavailable energy/environment/relay/analog/MQTT readings as NaN with availability indicators.
- Cache SD filesystem/card capacity, usage and log size every 30 seconds; count logger/crash archive successes and failures.
- Add six Prometheus form behavior tests; all 28 interface tests pass.
- Update every project Markdown guide and add `PROMETHEUS.md` with complete metric definitions and collection setup.
- Hardware collection, power-loss persistence and SD removal tests remain pending.

## 1.12.0

- Add `crash_info` with the central's read/archive/verify/erase workflow.
- Adapt diagnostics to ESP32-P4 RISC-V registers and SDK stack dump; retain ELF identifiers.
- Verify the SD report byte for byte before erasing the internal crash record.
- Preserve the internal record when SD archival or verification fails.
- Split UDP reports into packets of at most 1200 bytes for the central receiver.
- Remove the OLED screensaver and its commands; keep the display on with eight-second page rotation.
- Keep rotation active after advancing a page; replace the web wake shortcut with automatic pages.
- Add crash command interface tests; all 22 browser simulation checks pass.

## 1.11.0

- Inventário geral de comandos alinhado com os recursos de diagnóstico, OTA,
  proteção do OLED e cartão SD documentados no projeto.
- Add `/logs` and a bounded `/api/logs` reader for `/log.txt` on the SD card.
- Read at most the last 8 KB and select 25, 100 or 250 trailing lines.
- Add manual/automatic refresh, local search, follow mode and explicit stale/error states.
- Keep polling from writing new log entries or waking the OLED.
- Add six log-page tests and a web wake test; all 20 browser simulation checks pass.
- Add `botao_status` to diagnose the configured button pin and its electrical state.
- Clarify onboard RST versus the external GPIO 2 user button; add a web wake shortcut.

## 1.10.0

- Add an automatic dim moving-clock screensaver after 60 seconds of inactivity.
- Turn the OLED controller off after four further minutes, keeping device services running.
- Wake on ordinary commands, display events or button presses; the first short press only wakes.
- Keep transfer/OTA progress visible and preserve long-press Wi-Fi reset behavior.
- Add `protetor` controls for activation, wake, automatic mode, timeout and status.
- Keep display access on the application core and use rollover-safe inactivity timers.

## 1.9.0

- Receive serial commands on core 0 through a bounded eight-command queue.
- Run web, UDP, OLED, SD and command execution in an explicit task on core 1.
- Add `cores`, `ota_info` and serial-only `ota_password` diagnostics.
- Add authenticated browser firmware uploads and Arduino IDE OTA.
- Generate and verify a persistent per-device OTA password, disclosed only locally.
- Validate upload size, inactive OTA partition, complete writes and final image.
- Defer reboot until after success response and suspend commands during updating.
- Add six browser upload checks, bringing the interface test suite to 13 checks.

## 1.8.0

- Add bounded serial command input at 115200 baud with LF, CR and CRLF support.
- Route replies to the originating serial, UDP or HTTP channel.
- Add the web terminal with quick actions, history and redacted Wi-Fi credentials.
- Refresh dashboard cards every five seconds without replacing terminal input.
- Add pause/resume, connection failure feedback and confirmation for web mutations.
- Bound HTTP command responses to 8 KB and defer command-triggered reboots.
- Avoid the default five-second clock wait when logging without NTP sync.
- Add seven automated browser behavior checks; physical board tests remain required.

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
