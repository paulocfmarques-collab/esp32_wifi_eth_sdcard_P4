# Radar com acompanhamento — v1.24.0

**Gravação de diagnóstico (v1.25.3):** use `radar_log:on`, reproduza a oscilação e envie `radar_log:off`. Consulte `radar_log:status` para ver o nome do CSV. Baixe a sessão mais recente em `http://IP_DA_PLACA/radar.csv` antes de reiniciar, ou recupere o arquivo `radar-*.csv` diretamente do SD. Cada início cria um novo arquivo e para automaticamente após 120 s. O CSV separa o ângulo no momento da medição do ângulo atual, registra mudanças de posição, distância do lote antes do filtro de saltos, distância aceita, modo, tempo desde a inicialização e motivos de rejeição. Campos vazios indicam dado indisponível. São lotes concluídos, não pulsos individuais.

**Ajuste na v1.25.2:** antes de seguir um alvo, o radar completa um percurso entre os extremos, escolhe o eco confirmado mais próximo na busca e verifica novamente essa direção. Com eco central compatível, mantém a câmera parada; só sonda os lados se ficar cerca de 850 ms sem confirmação central. O ultrassom não distingue pessoas de outros objetos.

**Ajuste na v1.25.1:** a busca mantém o limite de 600 ms por posição sem candidato. Após o primeiro lote válido na faixa de 10–200 cm, o radar aguarda até mais 1.200 ms por outro lote compatível, tolerando lotes inválidos nesse intervalo. Com alvo confirmado e eco no centro, mantém a câmera nessa direção por pelo menos 800 ms antes de sondar os lados. O rastreamento inicia desabilitado: use `servo:rastrear:on` e `servo:on`.

## Tela sonar permanente — 1.25.0 (06/11/2026)

Nova oitava tela OLED: sonar semicircular com escala de 4 m, direção do servo, ecos reais por ângulo e marcador de alvo acompanhado. Permanece até o botão ou navegação explícita, sem protetor automático nem sobreposição do terminal; OTA tem prioridade. Abra com tela:7 ou pelo atalho Sonar no OLED no portal/Home Assistant. Botão sai para o relógio. Não liga o motor automaticamente. [Funcionamento e validação da tela sonar](SONAR_OLED.md). Firmware não compilado nem gravado; teste físico pendente.

Revisão: 06/11/2026. Implementado no firmware, ainda sem compilação, gravação ou validação física desta versão.

## Comportamento

O servo inicia desligado. O acompanhamento fica habilitado por padrão e entra em operação com `servo:on`, desde que as medições do HC-SR04 estejam habilitadas.

| Estado | Comportamento |
| --- | --- |
| Busca | Varre 5°–175° em passos de 5°, após cada tentativa de medição concluída na posição atual. Aguarda 100 ms de estabilização e até 30 ms pelo eco. Um percurso sem detecção tende a levar aproximadamente 4–5 s, conforme a duração dos ecos e a carga da aplicação. |
| Confirmando | Ao encontrar leitura entre 10 e 200 cm, mantém a direção para obter uma segunda leitura independente. As duas distâncias precisam diferir no máximo 25 cm. |
| Acompanhando | Aponta mecanicamente a câmera para a direção confirmada e mede centro, centro−6° e centro+6°. Mantém o centro se o eco lateral não for pelo menos 5 cm mais próximo. Se o centro perder o eco e uma lateral ainda enxergar um alvo compatível, muda o centro para essa lateral. |
| Recuperação | Se nenhum dos três pontos retornar distância compatível, amplia as sondagens para ±12°. Após três ciclos sem eco compatível, ou 2,5 s sem confirmação, volta à busca ampla. |
| Fixo | Um comando manual de ângulo interrompe a busca/acompanhamento e mantém aquela posição. `servo:on` retoma a busca. |

A distância do alvo pode mudar gradualmente. Para reduzir saltos para outro eco, cada sondagem aceita diferença de até o maior entre 30 cm e 30% da última distância acompanhada. Todos os ângulos são limitados a 5°–175°, inclusive perto das extremidades.

Cada aquisição tem uma sequência e o instante do comando de posição associado: uma leitura antiga não pode confirmar um novo ângulo e a mesma leitura não é contada duas vezes. Timeouts permitem avançar a busca; não se transformam em distância zero. O servo não avança automaticamente enquanto o ECHO está sendo medido.

Com o HC-SR04 habilitado, a busca não espera o streaming: o sensor governa os passos e a câmera transmite com alvo de 5 fps. Ao detectar um alvo, a varredura ampla cessa e a câmera permanece na região sondada. Sem medições habilitadas, o firmware retoma a varredura anterior de até 2° por quadro enviado, mínimo de 200 ms. OTA pausa o controle e descarta a confirmação de alvo; depois recomeça a busca.

## Comandos

- `ultrasonic:on`: habilita medições, necessário para a busca rápida/acompanhamento.
- `servo:on`: liga o servo e inicia a busca. Se acompanhamento estiver desabilitado, faz apenas a varredura rápida.
- `servo:rastrear:on`: habilita acompanhamento; não liga o motor sozinho. Se já estiver varrendo, começa a procurar um alvo.
- `servo:rastrear:off`: desabilita acompanhamento e libera a varredura ampla.
- `servo:angulo:90`: assume controle manual e posiciona no centro.
- `servo:off`: remove pulsos do servo e descarta o alvo.
- `servo:status`: mostra estado (`varrendo`, `confirmando`, `acompanhando`, `fixo`, `desligado`, `pausado`, `falha`), faixa de detecção, direção e distância acompanhadas.

Ativação e parâmetros não são salvos em prefs nesta versão. O motor continua desligado após reiniciar. Constantes de passo, sondagem e faixa de distância ficam em DeviceConfig.h.

## Home Assistant e Prometheus

O Home Assistant ganha botões **Habilitar acompanhamento** / **Desabilitar acompanhamento** e indicador **Alvo no radar**. O indicador representa um eco confirmado na faixa configurada, sem classificar pessoa ou movimento. Entidades antigas mantêm seus IDs.

O tópico MQTT `esp32/<id>/servo/state` inclui `tracking_enabled`, `target_locked`, `target_angle`, `target_distance_cm` e `search_step_degrees`, além dos dados de distância/servo existentes. Sem alvo, direção e distância acompanhadas são `null`. Os controles também funcionam pela Serial, UDP e terminal web.

Métricas adicionais: `radar_tracking_enabled`, `radar_target_locked`, `radar_target_distance_cm`, `radar_target_angle_degrees`. Sem alvo, distância e ângulo são NaN. Logs SD registram confirmação e perda do alvo; não cada sondagem.

## Limites e validação

O HC-SR04 fornece distância dentro de um cone, não identifica o objeto. O sistema faz acompanhamento aproximado em **um eixo**, sem visão computacional, zoom ou foco óptico automático. Uma parede dentro da faixa pode ser confirmada como alvo; alvos próximos podem ser confundidos. Objetos rápidos, superfícies inclinadas e ecos fracos podem levar à perda da direção. A sondagem local provoca pequenos movimentos visíveis na câmera.

Monte câmera e HC-SR04 alinhados na parte móvel do servo. O ângulo publicado é o comandado ao PWM, sem realimentação física. Os 100 ms de estabilização são uma hipótese inicial: ajuste após conferir a velocidade real do servo e a carga mecânica. Consulte [HC_SR04.md](HC_SR04.md) para ligações e divisor do ECHO.

Depois da atualização feita pelo usuário, teste em posição fixa com `ultrasonic:status`. Em seguida, use `servo:on`, aproxime um objeto a 10–200 cm e confira a transição de `confirmando` para `acompanhando`. Mova lentamente para os lados e depois retire o objeto: confira a recuperação da busca. Ainda não houve validação deste acompanhamento em hardware.
