# HC-SR04 — ESP32-P4 v1.25.0

## Tela sonar permanente — 1.25.0 (06/11/2026)

Nova oitava tela OLED: sonar semicircular com escala de 4 m, direção do servo, ecos reais por ângulo e marcador de alvo acompanhado. Permanece até o botão ou navegação explícita, sem protetor automático nem sobreposição do terminal; OTA tem prioridade. Abra com tela:7 ou pelo atalho Sonar no OLED no portal/Home Assistant. Botão sai para o relógio. Não liga o motor automaticamente. [Funcionamento e validação da tela sonar](SONAR_OLED.md). Firmware não compilado nem gravado; teste físico pendente.

Revisão: 06/11/2026. Implementação preparada; firmware não compilado nem gravado, sem teste físico do sensor.

## Ligação

| HC-SR04 | ESP32-P4 / ligação |
| --- | --- |
| VCC | 5 V, por exemplo P6 pino físico 2 |
| GND | GND comum da placa e da fonte do servo, por exemplo P6 pino físico 20 |
| TRIG | GPIO3, P6 pino físico 19 |
| ECHO | Divisor resistivo abaixo → GPIO4, P6 pino físico 18 |

O ECHO do HC-SR04 convencional usa lógica de 5 V e **não deve ser ligado diretamente ao ESP32**. A entrada TRIG aceita sinal de 3,3 V. Referência: [Adafruit — pinagem do HC-SR04](https://learn.adafruit.com/ultrasonic-sonar-distance-sensors/pinouts).

```text
HC-SR04 ECHO ---- resistor 2,2 kΩ ----+---- GPIO4 (P6 físico 18)
                                    |
                              resistor 3,3 kΩ
                                    |
                                   GND

HC-SR04 TRIG ----------------------------- GPIO3 (P6 físico 19)
HC-SR04 VCC ------------------------------ 5 V (P6 físico 2)
HC-SR04 GND ------------------------------ GND (P6 físico 20)
```

O divisor acima entrega aproximadamente 3,0 V para ECHO de 5,0 V. Com os resistores de 1 kΩ e 2 kΩ informados pelo usuário, a alternativa é ECHO → 1 kΩ → GPIO4 e GPIO4 → 2 kΩ → GND, entregando aproximadamente 3,33 V para ECHO de 5,0 V. Os dois resistores são necessários, na ordem indicada. Monte com a alimentação desligada e confira a identificação dos pinos no módulo. GPIO não corresponde ao número físico do conector. Servo permanece no GPIO2 (P6 físico 21) e usa fonte externa adequada; mantenha todos os GNDs comuns. Evite alimentar o servo pela linha 3V3 ou pelo GPIO.

Para associar distância e ângulo, prenda o ultrassom na parte que gira com o servo, alinhado à câmera, sem obstruir os transdutores. O firmware não consegue detectar como o sensor foi montado e não tem realimentação da posição física do servo.

## Leitura e varredura

- Habilitado ao iniciar. Configurado não significa que o sensor foi detectado: só um eco válido torna a leitura disponível.
- TRIG de 12 µs; bordas ECHO capturadas por interrupção; timeout de 30 ms. Sem `pulseIn()` e sem espera pelo eco no ciclo principal.
- Intervalo mínimo entre disparos de 100 ms. A folha de dados recomenda mais de 60 ms: [HC-SR04 — datasheet](https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf).
- Conversão nominal em centímetros: duração do ECHO em µs / 58. Aceita 2–400 cm. Sem compensação da velocidade do som pela temperatura.
- Espera pelo menos 100 ms desde o último comando de movimento antes de disparar. Durante a espera pelo eco, bloqueia apenas o avanço automático do servo, não a rede ou o vídeo.
- Distância e ângulo são registrados na mesma aquisição. Se um comando alterar o servo durante o eco, o ângulo dessa leitura fica desconhecido. Com servo desligado, a distância continua disponível, mas o ângulo é desconhecido.
- Sem eco, ECHO preso em HIGH, fora da faixa ou leitura com mais de 1 s: distância indisponível (`null` no JSON/MQTT, `NaN` no Prometheus), nunca um zero inventado.
- O servo mantém faixa 5°–175°. Com medições habilitadas, busca em passos de 5° por leitura concluída e acompanha um alvo confirmado conforme [RADAR_TRACKING.md](RADAR_TRACKING.md). Sem medições habilitadas, usa até 2° por quadro enviado e mínimo de 200 ms. Um movimento em posição fixa só é medido quando terminar e estabilizar.
- Leituras pausam em OTA. Resumo no SD a cada minuto; sem gravar cada pulso. Contadores em RAM reiniciam com a placa.

Distância e ângulo alimentam o acompanhamento aproximado descrito em [RADAR_TRACKING.md](RADAR_TRACKING.md). Não há identificação visual do objeto, mapa persistente ou desvio de obstáculos.

## Comandos

| Comando | Função |
| --- | --- |
| `ultrasonic:status` | JSON com distância, ângulo comandado da aquisição, idade, disponibilidade, contadores e erro |
| `ultrasonic:on` | Habilita medições |
| `ultrasonic:off` | Desabilita medições e invalida o último resultado |
| `servo:on` | Inicia varredura |
| `servo:off` | Remove pulsos do servo; não desliga a medição de distância |
| `servo:angulo:90` | Posiciona gradualmente no centro |

Use Serial, UDP ou terminal web. O portal tem atalho **Distância ultrassônica** para consultar o diagnóstico.

## Home Assistant e MQTT

Com MQTT configurado, a descoberta anuncia sensores **Distancia ultrassonica** (cm) e **Angulo da leitura ultrassonica** (°), além dos botões Ligar ultrassom, Desligar ultrassom e Atualizar ultrassom. Entidades anteriores mantêm seus IDs.

O tópico `esp32/<id>/servo/state` publica a cada segundo:

- `distance_cm`, `distance_angle`: distância e ângulo da mesma aquisição; `null` se indisponíveis.
- `ultrasonic_available`, `ultrasonic_age_ms`, `ultrasonic_samples`, `ultrasonic_errors`: diagnóstico da medição.
- `angle`: ângulo atual comandado ao servo; pode ser diferente de `distance_angle`, que pertence à aquisição anterior.

Comandos em `esp32/<id>/command`, não retidos: `ultrasonic:on`, `ultrasonic:off`, `ultrasonic:status`. Recepção apenas dos comandos permitidos, com execução no ciclo principal. Nenhum movimento autônomo adicional é comandado pelo sensor.

## Prometheus

Na porta configurada para métricas, `/metrics` inclui:

| Métrica | Significado |
| --- | --- |
| `ultrasonic_available` | 1 com distância recente e válida; caso contrário 0 |
| `ultrasonic_distance_cm` | Distância válida ou NaN |
| `ultrasonic_servo_angle_degrees` | Ângulo comandado da aquisição ou NaN |
| `ultrasonic_samples_total` | Leituras dentro da faixa |
| `ultrasonic_errors_total` | Timeouts, ECHO alto ou leituras fora da faixa |
| `ultrasonic_sample_age_seconds` | Idade da última tentativa concluída; NaN antes da primeira |

Depois da montagem e da atualização feita pelo usuário, consulte `ultrasonic:status` com um objeto conhecido à frente. Confira `available=true`, distância plausível e contadores avançando. Valide primeiro em posição fixa, depois com `servo:on`.
