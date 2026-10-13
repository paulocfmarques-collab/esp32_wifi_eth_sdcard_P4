# ESP32-P4 — OLED Network Monitor

Firmware v1.11.0, based on the supplied Wi-Fi / Ethernet / SD project.
Hardware pins and all existing commands are preserved. Use the original board,
ESP32 Arduino core and libraries configured for your ESP32-P4 and Wi-Fi companion.
This update does not configure the companion or change SDMMC wiring.

## Página de log — 1.11.0

Abra `/logs` ou clique em **Log do dispositivo** no menu. A página consulta
`/log.txt` no cartão SD e mostra as últimas 25, 100 ou 250 linhas, com atualização
manual, atualização automática a cada cinco segundos, busca local e opção de
acompanhar o final. Somente os últimos 8 KB são lidos, por posicionamento direto
no arquivo, para limitar uso de memória e tempo de leitura de arquivos grandes.
O conteúdo é exibido como texto; HTML no log não é executado.

`GET /api/logs?lines=100` retorna JSON com texto, tamanho do arquivo, número de
linhas e indicação de trecho limitado. A leitura não grava outro evento no SD
nem acorda o OLED. A atualização automática pausa em abas ocultas. Erros do SD,
arquivo ausente e atualização OTA são informados; se houver leitura anterior,
ela permanece visível com aviso de que pode estar desatualizada.

Testes de interface: `node --test tests/portal.test.cjs tests/firmware.test.cjs tests/logs.test.cjs`.
Os 20 testes passaram em simulação de navegador, incluindo o atalho para acordar
o OLED; leitura real do cartão requer a placa.

Compilação final da versão 1.11.0 concluída com Arduino ESP32 3.3.12 e alvo
`esp32:esp32:esp32p4`, opções padrão: **1.223.348 bytes (93%)** de programa e
39.804 bytes de variáveis globais. Não foi gravada nem testada fisicamente na placa.

## Proteção do OLED — 1.10.0

O botão de despertar deste projeto é externo, entre GPIO 2 e GND. Os botões
integrados da Waveshare ESP32-P4-WIFI6 são BOOT e RST; RST reinicia a placa e não
gera o evento esperado no GPIO 2. BOOT/GPIO 35 compartilha Ethernet TXD1 e não foi
reconfigurado. O terminal web oferece **Acordar tela** (`protetor:wake`);
`botao_status` permite diagnosticar o botão externo.

Após 60 segundos de inatividade, um relógio em movimento substitui o painel e o
OLED reduz o brilho. Quatro minutos depois, o controlador apaga o display; os
serviços do ESP32 continuam ativos. O primeiro toque no botão acorda sem mudar a
página. Comandos normais e logs de conexão também acordam; rotação de páginas e
consultas automáticas do navegador não contam como interação. Transferências e
OTA mantêm o progresso visível.

Comandos em todos os canais: `protetor:on`, `protetor:off`, `protetor:auto`,
`protetor:wake`, `protetor:status` e `protetor:tempo:N` (15 a 3600 segundos).
O status não acorda o OLED. As opções valem até reiniciar, quando retorna o padrão
automático de 60 segundos. O botão longo e os dois cores mantêm suas funções.
O funcionamento do relógio, brilho, desligamento e botão requer teste na placa.

A versão 1.10.0 compilou sem erros com Arduino ESP32 3.3.12, alvo
`esp32:esp32:esp32p4` e opções padrão: 1.215.452 bytes de programa (92% da partição)
e 39.804 bytes de variáveis globais. Não foi gravada nem testada fisicamente na placa.

## Dois cores e atualização pela rede — 1.9.0

O firmware cria uma tarefa de recepção serial no **core 0** e uma tarefa do
aplicativo no **core 1**. A recepção usa uma fila de oito comandos de 254 bytes;
se a fila encher, o comando inteiro é descartado e a serial informa a quantidade.
O core 1 executa comandos e coordena web, UDP, SD, OLED, LED e OTA. Isso permite
receber serial durante operações demoradas sem acessar OLED/SD por duas tarefas
ao mesmo tempo. Se a criação de uma tarefa falhar, o firmware informa a falha e
usa o loop como alternativa. Consulte `cores` para verificar a execução.

Atualização pelo **navegador**: abra `/firmware`, use o usuário `admin` e a senha
mostrada na serial com o prefixo `[OTA]`. Também é possível consultar a senha com
`ota_password` **somente pela serial local**. A senha é aleatória por dispositivo,
gravada e verificada em Preferences e não aparece no portal, UDP ou logs do SD.
Caso a persistência falhe, a atualização fica desabilitada. Envie apenas o `.bin`
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

Compilação final concluída com Arduino ESP32 **3.3.12**, alvo
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

| Command | Page |
| --- | --- |
| `tela:0` | Clock, date and interface status |
| `tela:1` | Wi-Fi SSID, IP, gateway, RSSI and signal bars |
| `tela:2` | Ethernet status, IP, gateway, disconnect count and offline time |
| `tela:3` | CPU, free/minimum heap, SD mount status and uptime |
| `tela:4` | Gateway RTT for each interface, timeout percentage and history graph |
| `tela:5` | Four most recent connection events, timestamped since boot |
| `tela:next` | Advance one page and stop automatic rotation |
| `tela:auto` | Resume rotation every eight seconds |
| `net_monitor` | Retrieve network statistics and recent events via UDP |

A command or network log temporarily displays the original console for five
seconds. File transfer progress keeps the terminal visible until completion.
Selecting a page takes effect immediately on the next display refresh.

## Physical button

GPIO 2, wired to ground with INPUT_PULLUP:

- Short press and release: advance one page, disabling automatic rotation.
- Hold for five seconds, then release: clear saved Wi-Fi settings and restart.

The `reset_wifi` command still clears Wi-Fi settings directly. The hardware EN
reset button is independent of GPIO 2. Firmware and SD files are not erased by
this settings operation.

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
and source files together, including the two new `NetworkMonitor` files.

Validation performed: source structure, local includes, command preservation,
page integration and ZIP integrity. See the latest validation notes above for
compilation and interface tests. Physical board tests remain pending.
Recommended board checks: each page, both links together, cable disconnect,
Wi-Fi disconnect, gateway filtering ICMP, AP-only boot, SD commands and short/
long button presses.

API references: [Espressif ICMP Echo](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/icmp_echo.html)
and [ESP-NETIF interface index](https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/api-reference/network/esp_netif.html).

## Version 1.5.1

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
