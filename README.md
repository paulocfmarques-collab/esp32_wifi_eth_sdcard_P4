# ESP32-P4 — OLED Network Monitor


## Clima externo — 1.15.0

Temperatura e umidade de Porto Alegre vêm do [Open-Meteo](https://open-meteo.com/en/docs), nas coordenadas -30.03283, -51.23019, iguais às do M5NanoC6. São condições estimadas pelo serviço meteorológico; a temperatura interna da CPU permanece separada.

A primeira consulta aguarda internet por Wi-Fi ou Ethernet e relógio sincronizado por NTP. Atualiza a cada 10 minutos; em caso de falha, tenta novamente após 2 minutos. O cache expira em 30 minutos, considerando também o horário informado pela API; dados ausentes ou vencidos viram `NaN`. A consulta HTTPS roda em tarefa própria no core 0, com resposta limitada a 2048 bytes. `/metrics` lê somente o cache. Novas consultas são suspensas durante OTA; uma consulta já iniciada pode terminar.

Use `weather_status` pela Serial, UDP ou terminal web para ver rede, NTP, validade, temperatura, umidade, idade e último resultado HTTP. Instale **ArduinoJson 7.x** (revisado com os cabeçalhos 7.4.3); HTTPClient e NetworkClientSecure acompanham o pacote Arduino ESP32.

Firmware v1.15.0, based on the supplied Wi-Fi / Ethernet / SD project.
Hardware pins follow DeviceConfig.h; the navigation button now uses GPIO27. Use the original board,
ESP32 Arduino core and libraries configured for your ESP32-P4 and Wi-Fi companion.
This update does not configure the companion or change SDMMC wiring.

Documentação revisada em **19/10/2026**, para o firmware **1.15.0**.

Estado atual: botão **GPIO27/GND** para acordar e avançar; carinhas de robô em
toda a tela após **60 s** sem interação; bip nas trocas de página pelo ES8311.
Segurar o botão não apaga Wi-Fi. A web permanece na porta 80 e as métricas na
porta configurada. Consulte [comandos](COMANDOS.md), [guia em português](LEIA-ME.md)
e [Prometheus/Grafana](PROMETHEUS.md).

Validação mais recente: **36 testes de interface passaram**. Esta revisão não compila nem grava o firmware; botão, áudio e saída do protetor
ainda precisam ser conferidos na placa. As notas de versões abaixo são históricas
quando identificadas como tal.

## Revisão do áudio — 1.14.1

Os documentos da ESP32-P4-Module-DEV-KIT confirmam o circuito **ES8311 + NS4150B**,
I²C SDA7/SCL8, MCLK13, BCLK12, LRCK10, saída do ESP32 em GPIO9 e enable do
amplificador em GPIO53. O GPIO11 é a saída do codec para captura, não a reprodução.
Não foi necessário mudar os pinos. O usuário confirmou que o alto-falante está conectado.

O driver agora reinicia o codec, confere o formato/DAC por leitura dos registradores,
preserva os demais bits ao remover mute e transmite a mesma amostra em ambos os
slots I2S de 16 bits, a 48 kHz. O amplificador recebe cerca de **100 ms de silêncio
clockado antes do tom**, mais silêncio ao final, em sua tarefa própria.
Escritas parciais são completadas com espera limitada; um timeout de reprodução
é registrado sem desativar permanentemente a próxima tentativa.

Essas mudanças tratam possíveis causas no software. Ainda não há confirmação de
qual delas explica o silêncio nesta placa, nem teste físico da correção.

Após compilar e gravar, use o botão **Testar som** no terminal web ou envie:

```text
audio_test
audio_status
```

audio_test solicita um tom de **500 ms / 880 Hz** sem mudar a página. Aguarde
cerca de um segundo antes de consultar audio_status. O status mostra inicialização,
erro, solicitações, envios completos, falhas de escrita, bytes do último envio e
registradores do ES8311. **Envio completo não comprova som audível**: valida apenas
a entrega dos dados ao I2S. O GPIO53 normalmente já estará LOW ao consultar após
o tom, pois o amplificador volta a desligar. Os contadores ficam em RAM e as
solicitações rápidas podem ser agrupadas.

Se continuar sem som, guarde a saída completa de audio_status e as mensagens
[AUDIO] da serial. Os **35 testes da interface passaram**; não houve compilação,
gravação ou teste sonoro físico pelo assistente, conforme combinado.

Referência do exemplo da placa:
[I2SCodec oficial da Waveshare](https://github.com/waveshareteam/ESP32-P4-Platform/blob/main/examples/esp-idf/12_I2SCodec/main/i2s_es8311_example.c).
Fontes locais consultadas: esquema ESP32-P4-Module-DEV-KIT.pdf, página 1
(Codec / PA&SPEAKER&MIC), e documentação Pinout_Definition, páginas 4–5.
Os PDFs foram apenas lidos, sem modificações.

## Temperatura da CPU — 1.13.2

O exporter inclui `esp_cpu_temperature_celsius`, em °C, para exibir no Grafana.
Use essa métrica na fonte Prometheus, com unidade Celsius. O comando `temp`
compartilha a leitura do sensor; falhas retornam indisponível/NaN.
Veja [PROMETHEUS.md](PROMETHEUS.md) para a consulta do painel.
Compilação e teste físico desta revisão ficam a cargo do usuário.

## Prometheus — 1.13.1

O padrão de conexão segue o projeto M5NanoC6: endereço do servidor, porta de
métricas da placa e intervalo de coleta (5–3600 s), salvos e conferidos nas
Preferences pela página /prometheus. A web e a API permanecem na porta 80.
A mudança de porta é aplicada cerca de um segundo após salvar, sem reiniciar.

Na primeira instalação, /metrics compartilha a porta 80. Ao escolher outra porta,
somente ela atende as métricas; painel, configuração e download continuam na 80.
A porta 3232 permanece reservada para OTA. As configurações anteriores preservam
endereço e porta, usando intervalo inicial de 15 s; a coleta passa a ser sempre ativa.

A página mostra o YAML de coleta, permite baixar /prometheus.yml e oferece um
link para as métricas da placa. O Prometheus consulta a placa; salvar não altera
o servidor automaticamente. Não há mais campo de porta do servidor nem chave de ativação.
Todas as métricas existentes, inclusive SD, foram mantidas.

Consulte [PROMETHEUS.md](PROMETHEUS.md) para API, configuração e catálogo.
Os 36 testes de interface passaram na validação mais recente. A compilação será feita pelo usuário.

## crash_info — 1.12.0

O comando `crash_info` segue o central: informa o motivo do último reset, lê o
core dump válido, arquiva o relatório no SD e só apaga o registro interno após
reabrir o arquivo e conferir tamanho e conteúdo byte a byte. Sem SD, com gravação
incompleta ou falha na conferência, o registro interno permanece para nova tentativa.

O relatório mostra panic, tarefa, PC e identificador SHA256 do ELF. No ESP32-P4
(RISC-V), inclui RA, SP, MCAUSE, MTVAL e a pilha bruta fornecida pelo SDK; a
decodificação do backtrace exige o ELF do firmware que sofreu o crash. Preserve
esse arquivo junto de cada versão. Não são exibidos endereços da pilha como se
fossem um backtrace já decodificado.

Os arquivos são `/crash_YYYYMMDD_HHMMSS.txt` ou `/crash_SEM_NTP_<uptime>.txt`,
com sufixo numérico quando necessário. Use `sd_list` e `sd_read:/crash_....txt`
para consultá-los. A página `/logs` continua lendo apenas `/log.txt`.

Disponível pela serial, UDP e terminal web. No UDP, o relatório é dividido em
pacotes de até 1200 bytes para caber no receptor do central; a entrega continua
sujeita às perdas e à ordem do UDP. No navegador, a operação pede confirmação,
pois arquiva dados e apaga o registro interno depois da verificação.

Sem crash válido, informa essa condição e o último reset, sem criar arquivo ou
apagar dados. Se o SDK não habilitar core dump em flash/ELF, informa que o recurso
está desabilitado. O SDK ESP32-P4 3.3.12 e a tabela padrão usados no projeto já
incluem esse suporte e a partição coredump. O comando não provoca um crash.

## Página de log — 1.11.0

Abra `/logs` ou clique em **Log do dispositivo** no menu. A página consulta
`/log.txt` no cartão SD e mostra as últimas 25, 100 ou 250 linhas, com atualização
manual, atualização automática a cada cinco segundos, busca local e opção de
acompanhar o final. Somente os últimos 8 KB são lidos, por posicionamento direto
no arquivo, para limitar uso de memória e tempo de leitura de arquivos grandes.
O conteúdo é exibido como texto; HTML no log não é executado.

`GET /api/logs?lines=100` retorna JSON com texto, tamanho do arquivo, número de
linhas e indicação de trecho limitado. A leitura não grava outro evento no SD
nem altera a página do OLED. A atualização automática pausa em abas ocultas. Erros do SD,
arquivo ausente e atualização OTA são informados; se houver leitura anterior,
ela permanece visível com aviso de que pode estar desatualizada.

Testes de interface: `node --test tests/portal.test.cjs tests/firmware.test.cjs tests/logs.test.cjs tests/prometheus.test.cjs`.
Os 35 testes passaram em simulação de navegador, incluindo `crash_info`,
Prometheus e atalhos das carinhas/navegação. Leitura real do cartão requer a placa.

Última compilação concluída registrada: versão anterior 1.11.0, Arduino ESP32 3.3.12 e alvo
`esp32:esp32:esp32p4`, opções padrão: **1.223.348 bytes (93%)** de programa e
39.804 bytes de variáveis globais. Não foi gravada nem testada fisicamente na placa.

## Botão e protetor de robôs — 1.14.0

O botão fica entre **GPIO27 e GND**, com INPUT_PULLUP: LOW = pressionado.
Uma pressão estável por 50 ms acorda o OLED e avança uma página, mantendo a rotação
automática de oito segundos. Segurar não repete nem apaga as redes Wi-Fi.
Use reset_wifi no terminal para limpar as redes; RST continua sendo reset de hardware.

Após **60 segundos sem interação**, carinhas de robô animadas ocupam todo o
OLED 128×64, com olhos móveis, piscadas e quatro expressões. O display permanece
ligado, sem entrar em sleep. O botão e comandos acordam; logs de rede, consultas
web/Prometheus e troca automática não reiniciam esse tempo. A animação não usa delay.
O progresso OTA tem prioridade. Há atalho **Carinhas de robô** no terminal web.

Comandos: protetor:on (agora), protetor:off (desativa o automático e acorda),
protetor:auto (reativa), protetor:status e protetor:tempo:SEGUNDOS (5–3600).
Tempo e ativação do protetor ficam em RAM e retornam ao padrão ao reiniciar.
protetor:status consulta sem acordar ou reiniciar o tempo; tela:next e tela:auto
permitem voltar à navegação pela web/serial/UDP.

### Som de navegação

Cada mudança real entre as seis páginas, automática ou manual, solicita um
bip de 80 ms, 1200 Hz, com suavização de início/fim. A animação das carinhas não
emite um som por quadro. O áudio roda numa tarefa própria no core 0; notificações
rápidas são agrupadas, sem fila ilimitada. Durante OTA, novos sons são suspensos.

Codec **ES8311, I²C 0x18**, compartilhando Wire/SDA7/SCL8 com o OLED. I2S:
**MCLK13, BCLK12, LRCK10, DOUT9**, 48 kHz, dois slots com sinal duplicado, 16 bits.
O amplificador usa **GPIO53**, ativo em HIGH durante a estabilização, o bip e o silêncio final.
Não há captura de microfone. O áudio não inicializa outro barramento I²C.
Sem codec, a navegação continua sem som. Falhas de reprodução ficam no status; consulte audio_status
e as mensagens [AUDIO] na serial. Não foi adicionada biblioteca externa de áudio.

Referência da sequência de registradores:
[exemplo oficial ES8311 da Waveshare](https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/tree/main/examples/Arduino/i2s).
Os pinos seguem os informados para esta placa; outros modelos Waveshare podem diferir.

Os 35 testes da interface passaram. Compilação e testes físicos de botão, saída
do protetor, reprodução sonora e OTA ficam a cargo do usuário, conforme combinado.

## Dois cores e atualização pela rede — 1.9.0

O firmware cria uma tarefa de recepção serial no **core 0** e uma tarefa do
aplicativo no **core 1**. A recepção usa uma fila de oito comandos de 254 bytes;
se a fila encher, o comando inteiro é descartado e a serial informa a quantidade.
Desde 1.14.0, o core 0 também executa a tarefa `page_audio`: ela recebe notificações
de troca e transmite o bip por I2S, sem acessar o OLED ou o barramento I²C.
O core 1 executa comandos e coordena web, UDP, SD, OLED, LED e OTA. Isso permite
receber serial durante operações demoradas sem acessar OLED/SD por duas tarefas
ao mesmo tempo. Se a criação de uma tarefa falhar, o firmware informa a falha e
usa o loop como alternativa. Consulte `cores` para verificar a execução.

Atualização pelo **navegador**: abra `/firmware`, use o usuário `admin` e a senha
fixa `123456`, também usada pelo Arduino IDE. Não há geração nem leitura de
senha OTA nas Preferences; senhas antigas gravadas são ignoradas.
`ota_password` continua disponível somente pela serial local.
Essa senha padrão é fraca: use apenas em rede confiável. Envie apenas o `.bin`
do aplicativo exportado pelo Arduino IDE, compilado para a mesma placa e
configuração. Bootloader, tabelas de partições e imagens merged não são arquivos
de atualização do aplicativo.

Atualização pelo **Arduino IDE**: escolha a porta de rede `esp32-p4-node` e use a
mesma senha. ArduinoOTA usa a porta 3232 e anuncia a placa via mDNS; computador e
placa devem estar na mesma rede. Use `ota_info` para consultar o serviço e o tamanho
da partição OTA. O upload pelo navegador também funciona pelo AP de configuração.

A primeira instalação desta versão é por **USB**, com um esquema de partições
que inclua `otadata` e dois slots de aplicativo. A tabela padrão de 4 MB testada
na compilação tem dois slots de 1.310.720 bytes; escolha outra tabela compatível
com a flash real da placa se seu aplicativo exceder esse limite. Atualizações OTA
não alteram a tabela de partições nem configuram o companion Wi-Fi.

Uploads autenticados pelo navegador informam o tamanho exato e são escritos na
partição OTA inativa. Escritas incompletas, arquivos grandes, falta de partição
e falhas de verificação são rejeitados; o boot só muda após `Update.end(false)`
confirmar a imagem. A placa responde antes de reiniciar. A execução de comandos
fica suspensa durante OTA; a recepção serial continua na fila limitada. Não há
rollback automático de um firmware que inicia e depois falha em execução.
O portal HTTP e OTA se destinam a uma rede local confiável, sem exposição direta
à Internet. Mantenha a alimentação durante a atualização.

Verificação das interfaces: `node --test tests/portal.test.cjs tests/firmware.test.cjs`.
São 13 testes com simulação de navegador. Validação de upload real, interrupção,
USB, descoberta OTA no IDE e funcionamento dos dois cores requer a placa.

Registro histórico da compilação da versão 1.9.0: Arduino ESP32 **3.3.12**, alvo
`esp32:esp32:esp32p4` e opções padrão: **1.212.270 de 1.310.720 bytes (92%)**
de programa e **39.788 bytes** de variáveis globais. As pilhas das tarefas e as
alocações de rede são adicionais em tempo de execução. A compilação valida o
alvo padrão do módulo; selecione a revisão real do chip, USB e flash da sua placa
na instalação. Não foi realizada gravação ou teste físico da placa.

## Serial, painel e terminal web — 1.8.0

Abra `/info` no IP da placa. O painel atualiza seus cartões e histórico a cada
cinco segundos sem recarregar a página nem apagar o texto do terminal. É possível
pausar a atualização; abas ocultas suspendem as consultas. Falhas de comunicação
aparecem no painel, mantendo a última leitura visível até a conexão voltar.

O terminal web envia os mesmos comandos do UDP e da serial. Inclui atalhos,
histórico pelas setas para cima/baixo, limpeza da saída e confirmação para ações
que alteram arquivos ou configurações. `wifi_add` não armazena a senha no histórico
nem a mostra na saída. A resposta web tem limite de 8 KB; prefira `sd_tail` para
arquivos grandes. Uma falha ou timeout não garante que o comando deixou de executar:
confira o estado antes de repetir operações de escrita.

No Monitor Serial, selecione **115200 baud** e final de linha **LF, CR ou CRLF**.
Depois da inicialização, envie um comando por linha, por exemplo `help`, `status`,
`led_on`, `tela:next` ou `sd_list`. O limite é 254 bytes por comando em todos os
canais. Na serial, linhas maiores são descartadas inteiras até o próximo final
de linha; linhas vazias são ignoradas. As respostas voltam ao canal de origem;
mensagens de diagnóstico da placa continuam aparecendo na serial.

Para USB nativo, habilite **USB CDC On Boot** na configuração correspondente da
placa no Arduino IDE. Quando CDC está desabilitado, `Serial` usa a UART conforme
a configuração original. Nenhum pino foi alterado.

Endpoints locais: `GET /api/dashboard` retorna o fragmento HTML dos cartões;
`POST /api/command` recebe o campo `cmd` e exige o cabeçalho `X-ESP32-Command: 1`.
Esse cabeçalho impede comandos enviados por formulários de outros sites, mas não
substitui autenticação. O portal continua destinado a uma rede local confiável.
Reinícios por comando são agendados para permitir entregar a resposta HTTP.
Leituras extensas de arquivos SD ainda podem atrasar o atendimento de outras tarefas.

Verificação do terminal: `node --test tests/portal.test.cjs`. Os testes simulam
envio, limite em UTF-8, confirmação, sigilo de senha, atualização do painel e
falha de conexão. O teste físico de serial USB, UDP e navegador requer a placa.

## OLED pages

The SSD1306 128×64 display refreshes once per second and rotates pages every
eight seconds by default. Each frame is composed in the existing display buffer
before being transferred to the OLED. This is a monochrome display.
These page updates pause while the full-screen robot screensaver is active.

| Command | Page |
| --- | --- |
| `tela:0` | Clock, date and interface status |
| `tela:1` | Wi-Fi SSID, IP, gateway, RSSI and signal bars |
| `tela:2` | Ethernet status, IP, gateway, disconnect count and offline time |
| `tela:3` | CPU, free/minimum heap, SD mount status and uptime |
| `tela:4` | Gateway RTT for each interface, timeout percentage and history graph |
| `tela:5` | Four most recent connection events, timestamped since boot |
| `tela:next` | Advance one page and keep automatic rotation active |
| `tela:auto` | Resume rotation every eight seconds |
| `net_monitor` | Retrieve network statistics and recent events via UDP |

A user command temporarily displays the original console for five seconds.
Background network logs do not overwrite the robot while the screensaver is active.
File transfer progress wakes the display and keeps the terminal visible until completion.
Selecting a page takes effect immediately on the next display refresh.

## Physical button

GPIO27 to GND, INPUT_PULLUP. A debounced press wakes and advances once.
Holding the button never clears Wi-Fi; use reset_wifi explicitly.
See the 1.14.0 section above for robot screensaver and ES8311 audio behavior.

## Measurements

The monitor uses the native ESP-IDF ICMP API, without an additional ping library.
One asynchronous ping starts approximately every five seconds. With both links
online, Wi-Fi and Ethernet alternate, giving each a sample approximately every
ten seconds. With only one online, that interface is sampled every five seconds.
Each request is bound to its interface and targets that interface's gateway.
Timeout is one second. Bar height represents RTT on an adaptive scale; crosses
represent unanswered pings. Each interface retains 24 results.

A timeout can mean that the router filters ICMP; it does not establish that Wi-Fi
or Ethernet disconnected and does not measure Internet reachability. Connection
status, drops, offline duration and four recent events are observed by the main
loop. Statistics are stored in RAM and reset on reboot. Existing blocking SD,
LED and setup operations can delay observation of connection changes.

Ethernet now starts even when no saved Wi-Fi network connects. UDP is also opened
in AP mode. Network events are queued for processing in the main loop, avoiding
OLED I2C access from the event task. Gateway ping callbacks only publish results.
The existing Wi-Fi/Ethernet route priorities are applied as interfaces appear.
NTP settings are loaded on startup so synchronization can also use Ethernet.

## UDP and installation

UDP port: **4210**. HTTP portal: `/`, `/info`, `/wifi`. Existing system, LED,
time and SD commands remain available through `help`. Commands exceeding 254
bytes are rejected before parsing; this fixes the original buffer overflow.

Extract the ZIP and open `wifi_Eth_SDCard/wifi_Eth_SDCard.ino`. Keep all headers
and source files together, including `PageAudio.h/.cpp`, `CpuTemperature.h`,
`FirmwareVersion.h`, `PrometheusMetrics.h/.cpp` and `PrometheusUi.h`.
No extra audio library is required beyond the installed Arduino-ESP32 core.

Validation performed: source structure, local includes, command preservation,
page integration and ZIP integrity. See the latest validation notes above for
compilation and interface tests. Physical board tests remain pending.
Recommended board checks: each page, both links together, cable disconnect,
Wi-Fi disconnect, gateway filtering ICMP, AP-only boot, SD commands and short/
long button presses.

API references: [Espressif ICMP Echo](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/icmp_echo.html)
and [ESP-NETIF interface index](https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/api-reference/network/esp_netif.html).

## Version 1.5.1

Historical behavior below; the current GPIO27 navigation button never clears Wi-Fi.

Network reports now show successful RTT minimum/mean/maximum and sample age.
Timeouts are excluded from RTT averages. The HTTP `/info` page includes the
network report. OLED latency graphs show the selected interface's sample age.
LED blinking runs through the main loop without blocking network handling;
`led_on`/`led_off` cancel blinking. Holding the GPIO 2 button no longer blocks
network handling; releasing it after five seconds performs the Wi-Fi reset.
Single quotes are escaped in configuration page HTML attributes.

## Modern local portal — 1.6.0

Responsive `/info` and `/wifi` pages use self-contained styling and scripts.
Information cards show both links, system health, saved profiles and connection
history. The five-profile editor provides show/hide controls, active profile
badges and an explicit open-network option without exposing stored passwords.

The server validates the complete submission, verifies Preferences readback and
schedules restart only after success. Failed writes trigger a verified attempt
to restore the old profiles, with a visible error and no scheduled restart.
Multi-key updates are not power-loss atomic. Existing UDP commands are retained.

## Comandos — 1.7.0

Novos comandos: `wifi_add`, `wifi_list`, `wifi_status`, `eth_status`, `channel`,
`chip_info`, `heap_min`, `psram`, `health`, `selftest`, `ntp_status`, `sd_append`,
`sd_mkdir`, `sd_rmdir`; aliases `init` e `flash_info`. `net_info` inclui gateways,
máscaras e DNS de ambas as interfaces. Credenciais Wi-Fi são ocultadas dos logs.
`help` e `COMANDOS.md` foram atualizados; consulte o Markdown para sintaxe, exemplos
e os comandos anteriores. `set_fuso` aceita somente inteiros -12..14; `dst` somente
0 ou 1. Compile e teste na placa antes de usar esta versão.
