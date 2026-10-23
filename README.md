# ESP32-P4 — OLED Network Monitor

## USB Host e registros ampliados — 1.20.0

Abra **USB / Registros** no menu web (`/usb`) para ver host, teclado, mouse, pendrive, listagem da raiz e fila de gravação no SD. As USB-A usam o controlador **high-speed dedicado / TinyUSB root port 1**, preservando GPIO27 do botão e a interface de programação. Não se inicia a pilha ESP-IDF USB Host em paralelo com TinyUSB.

### Ligações e dispositivos suportados

- Jumper da base em **DEVICE**: ativa as **portas USB-A 2–4**, através do hub CH334. Esse nome é a marcação do seletor da placa; o firmware continua operando como host USB.
- Jumper em **HOST**: ativa a **porta USB-A 1 direta**. O hardware não oferece as quatro portas simultâneas nessa seleção. O firmware não movimenta o jumper.
- Teclado e mouse HID: drivers recebem relatórios, mostram conexão/remoção e contadores de atividade. Teclas/texto e movimentos individuais não são persistidos; há resumo de atividade a cada 10 s. O teclado não executa comandos automaticamente e o mouse não cria um cursor no OLED.
- Um pendrive MSC: montagem FAT16/FAT32 em /usb, **sem formatação automática** e sem escrita de arquivos pelo aplicativo. ExFAT/NTFS não são habilitados. Remoção encerra a montagem; falha inicial aguarda desconectar/reconectar para tentar montar novamente.
- A raiz fica em cache, com até 32 entradas e nomes de até 80 caracteres. **Reler raiz do pendrive** solicita nova leitura na tarefa USB; arquivos não são copiados para o SD.

Usa as classes USBHost, USBHostHIDKeyboard, USBHostHIDMouse e USBMSCFS do pacote Arduino ESP32 **3.3.12**, com suporte a hub já presente. Se o IDE estiver configurado para iniciar funções TinyUSB Device no boot em modo USB-OTG, o serviço informa conflito em vez de disputar o controlador. Use **USB Mode → Hardware CDC and JTAG**; mantenha CDC/MSC/DFU OTG Device no boot desativados quando aplicável. Não confundir um pendrive conectado como host com apresentar o SD ao PC como USB MSC Device: esta última função não foi adicionada.

### Comandos e API

- **usb_status**: estado do host (1 ativo, 2 falha de memória/tarefa, 3 pacote sem suporte, 4 conflito Device, 5 falha de inicialização), dispositivos, capacidade e erros.
- **usb_list**: listagem em cache da raiz do pendrive montado.
- **usb_rescan**: solicita releitura da raiz; conferir usb_list após alguns instantes. Não é reenumeração completa nem repetição da montagem FAT que falhou.
- **log_status**: fila, gravações, descartes e política de rotação.

Funcionam na Serial, UDP e terminal web. Rotas GET sem cache: `/api/usb/status`, `/api/usb/files`, `/api/log/status`. Releitura na página usa o endpoint de comandos já existente, POST protegido pelo header X-ESP32-Command. As operações de pendrive/dispatch HID rodam em tarefa própria, fora do atendimento HTTP e sem gravação SD em callbacks USB.

### O que passa a ser registrado no SD

- Boot, versão e motivo do reset; montagem inicial do SD.
- Eventos Wi-Fi/Ethernet, IP Wi-Fi obtido, comandos e metadados de recebimento/resposta UDP.
- Acessos HTTP e códigos/duração das respostas, sem corpo, argumentos de formulário, query string ou conteúdo de imagens. /metrics e /api/dashboard não geram uma linha por consulta para evitar registros repetitivos da própria coleta.
- Conexão/desconexão/erros MQTT com códigos de transporte/broker; nenhum payload de credenciais.
- Inicialização/falhas da câmera, início/fim dos streams, consulta Open-Meteo e falhas de áudio. Quadros não são gravados como binário no log.
- Início/fim/falhas de OTA, alertas/recuperações edge já existentes, conexão/remoção HID/MSC e resultado da montagem/leitura do pendrive.
- A cada 60 s: heap, mínimo de heap, PSRAM, temperatura CPU, tarefas, Wi-Fi/Ethernet/MQTT/RSSI, contadores UDP/MQTT/vídeo/USB e estado/capacidade/erros do SD.
- Avisos/erros do SDK emitidos pelo hook esp_log_set_vprintf, quando habilitados na compilação. Logs INFO/DEBUG do SDK, dumps de relatórios HID, early logs ROM/ISR, todas as chamadas Serial.print e conteúdo bruto de arquivos/imagens não são espelhados indiscriminadamente. Os eventos explícitos acima são registrados independentemente do nível INFO do SDK.

As mensagens explícitas entre tarefas passam por fila de **64 entradas** (até 199 bytes de texto por entrada), com uptime do evento. O ciclo da aplicação drena até quatro por segundo para limitar trabalho de I/O. A gravação inclui carimbo local, quando houver NTP; sem NTP, mantém indicação de ausência de hora e o uptime. Escritas do próprio logger suprimem a captura SDK para evitar recursão de erros SD. Há filtro de palavras relacionadas a senha/autorização/token e remoção de caracteres de controle; respostas com credenciais de OTA não são copiadas da Serial para o arquivo.

Isso não é garantia de registrar cada pacote ou cada evento em alta frequência. Fila cheia, falha/ausência do SD e perda de alimentação podem descartar mensagens; confira log_status e as métricas. Sem SD no boot, a fila é limitada e não há remontagem automática. A escrita é pausada enquanto a flash está em atualização, com tentativa limitada de drenagem após a conclusão; eventos ainda na RAM podem se perder ao reiniciar.

### Rotação e leitura

O arquivo atual continua **/log.txt**. Ao atingir 1 MiB, gira para **/log.1.txt** e mantém quatro históricos, até **/log.4.txt**; o mais antigo é removido na próxima rotação. Em operação normal, isso limita os logs a aproximadamente **5 MiB**, sem contar relatórios de crash ou outros arquivos do cartão. A rotação não formata o SD e verifica renomeações; falha interrompe a gravação daquele registro.

A página **Log do dispositivo** continua lendo o arquivo atual. Os arquivos históricos podem ser vistos/listados com os comandos SD existentes. log_status.written conta apenas a fila nova; sdcard_log_writes_total inclui também gravações diretas preexistentes.

Prometheus adiciona **usb_host_up**, **usb_hid_reports_total**, **sdcard_log_queue_dropped_total** e **sdcard_log_rotations_total**. Os contadores reiniciam no boot. Preferências, streaming, MQTT e partições OTA de 16 MB permanecem configurados como antes.

Validação: **75 testes de interface/partições passaram**. Incluem estado USB/FAT/fila, nomes de arquivos como texto, releitura com header protegido, falhas e bloqueio de consultas sobrepostas. Não houve compilação, montagem real de pendrive, teste físico de hub/HID, teste de carga ou de rotação em SD. Após compilar/gravar, conferir usb_status e log_status, conectar os dispositivos no grupo selecionado pelo jumper e verificar /log.txt.

Mapa de hardware: [pinos utilizados e esquema elétrico das ligações](MAPA_PINOS_E_ESQUEMA.md).

## Estabilidade da coleta durante vídeo — 1.19.1

Após o relato de interrupção no Home Assistant e Prometheus, a revisão remove o envio de JPEG do ciclo principal da web. Em 1.19.0, /camera.jpg ainda transmitia a imagem inteira por WebServer; um destinatário lento podia atrasar o atendimento de métricas e a publicação MQTT. Isso foi identificado no código, mas a causa completa do relato ainda depende de logs da placa.

- **/camera.jpg na porta 80 agora retorna HTTP 302** para **http://IP_DA_PLACA:PORTA_VIDEO/snapshot**. O cliente deve seguir o redirecionamento. O JPEG é enviado por uma tarefa do servidor da câmera. Para o Home Assistant, pode informar diretamente essa URL /snapshot na porta do vídeo, evitando o redirecionamento.
- O servidor admite até **dois streams + um snapshot em envio**; fotos simultâneas adicionais recebem 503 em vez de criar tarefas ilimitadas. O JPEG /snapshot permite CORS somente para leitura de imagem pelo navegador da porta 80; as APIs de configuração/comandos não mudam.
- Requests assíncronos concluídos liberam a posse e solicitam fechamento da sessão TCP, para recuperar os sockets.
- O limite de captura/envio foi reduzido de **5 para 2 imagens/s**, preservando resolução e qualidade JPEG 70. O timeout de rede continua em 2 s por operação; isso não é um prazo total para a sessão de vídeo.
- camera_status/API incluem **snapshot_clients**, **rejected_clients**, **heap_free** e **psram_free**, além dos contadores de streaming existentes. Recusas por limite não significam perda de pacote de MQTT/Prometheus.

No Home Assistant (MJPEG IP Camera), mantenha **MJPEG URL = http://IP_DA_PLACA:PORTA_VIDEO/stream** e use **Still Image URL = http://IP_DA_PLACA:PORTA_VIDEO/snapshot**. A porta normal é 81; confira o valor mostrado em /camera ou camera_status. Os endereços do broker MQTT e do alvo Prometheus permanecem os anteriores.

Validação: **69 testes de interface/partições passaram**. Não houve compilação, teste de carga da rede, conexão real ao Home Assistant/broker ou confirmação física da correção. Para validar na placa: com vídeo aberto, conferir /metrics na porta configurada e mqtt_status; depois fechar o vídeo e verificar a liberação de stream_clients. Se a interrupção persistir, registrar camera_status, mqtt_status e se a web continua respondendo. As preferências e a tabela de partições foram mantidas.

## Vídeo MJPEG e câmera no OLED — 1.19.0

A página **Câmera** (`/camera`, porta 80) agora inicia um fluxo MJPEG contínuo, com captura JPEG limitada a **5 imagens/s**, qualidade 70 e resolução do driver. Há botões para iniciar vídeo, obter foto JPEG e pausar/retomar a visualização. Ocultar ou fechar a página encerra a conexão de vídeo do navegador. Falha de conexão gera aviso e nova tentativa; a captura na placa continua independente do navegador.

### Servidor de vídeo

O endereço normal é **http://IP_DA_PLACA:81/stream**. Para preservar o Prometheus configurado, o firmware tenta portas TCP de 81 a 89 e pula a porta de métricas salva e portas que já estejam ocupadas. A URL exata aparece na página e em `camera_status` / `GET /api/camera/status` (stream_port e stream_path). Se todas estiverem ocupadas, stream_port é 0 e o JPEG continua disponível. Uma alteração no ambiente/porta do Prometheus pode mudar a porta de vídeo no próximo boot; confira a URL antes de cadastrar no Home Assistant. Não há nova preferência de porta de streaming.

O servidor de vídeo usa ESP HTTP Server com handlers assíncronos e uma tarefa por espectador, no máximo **dois clientes simultâneos**. Um terceiro recebe 503. O stream usa multipart/x-mixed-replace com frames image/jpeg, sem áudio. Cada cliente copia um JPEG recente para a PSRAM, libera o cache e envia pela própria tarefa, sem manter mutex durante a transferência. Timeout de envio de 2 s encerra clientes lentos; FPS real depende de rede, cena e processamento. Web/UDP/MQTT/Prometheus continuam em seus serviços anteriores. A porta ativa do vídeo passa a ser recusada na configuração do Prometheus para evitar colisão.

Durante OTA, a pausa da captura encerra streams existentes; snapshots e novas conexões sem imagem disponível recebem 503. Retomada do navegador usa reconexão automática. Não há gravação de vídeo no SD, envio das imagens por MQTT ou RTSP/H.264 nesta revisão.

### Home Assistant

Em **Configurações → Dispositivos e serviços → Adicionar integração → MJPEG IP Camera**, configure:

- **MJPEG URL:** http://IP_DA_PLACA:81/stream — substitua pela porta exibida na página.
- **Still Image URL:** http://IP_DA_PLACA/camera.jpg.
- Usuário e senha vazios, como no JPEG anterior.

Depois adicione a nova entidade ao painel. O Home Assistant e um navegador podem consumir as duas vagas de streaming. Referência: [integração MJPEG IP Camera](https://www.home-assistant.io/integrations/mjpeg/).

### Comando camera_oled

Envie **camera_oled** sem argumentos pela Serial, UDP ou terminal web; há também o atalho **Câmera no OLED** no terminal. O Home Assistant ganha um sexto botão MQTT, **Camera no OLED**, que usa o mesmo comando restrito.

O worker reduz o quadro RGB565 para **128×64 preto e branco**, com dithering e recorte central 2:1, preenchendo o SSD1306. Parte das bordas do enquadramento original pode ser cortada. O comando copia um quadro recente e o mostra por **10 segundos**, voltando depois às telas. GPIO27 ou tela:next sai antes. Outros comandos, mensagens de sistema, atualização de firmware ou ações de tela podem substituir a imagem. Não é vídeo no OLED e não muda as preferências do protetor/rotação. OLED ausente/ocupado ou câmera sem imagem recente retornam diagnóstico sem desligar a aplicação.

### Diagnóstico e validação

`camera_status` agora inclui stream_port, stream_path, stream_clients, stream_frames e stream_errors. Prometheus adiciona `camera_stream_clients` (gauge), `camera_stream_frames_total` e `camera_stream_errors_total` (counters). Frames contam envios concluídos por cliente; desconexões durante envio e timeouts podem incrementar erros e não significam necessariamente defeito no sensor. Contadores reiniciam no boot. Os frames JPEG da câmera continuam contados separadamente.

Mantidos partitions.csv para flash 16 MB, dois aplicativos OTA de 6,25 MiB e as preferências anteriores. **69 testes passaram** (interface e tabela de partições), incluindo stream com porta alternativa, pausa/visibilidade, snapshots, reserva da porta no Prometheus e atalho camera_oled. Não houve compilação nem teste físico do novo streaming/OLED. O usuário confirmou imagem JPEG boa na versão anterior.

## Espaço do firmware e partições — 1.18.1

O erro de compilação com máximo de **1966080 bytes** corresponde ao esquema Minimal SPIFFS (1.9MB APP). O firmware informado tem **2316966 bytes**, portanto esse esquema é pequeno demais; a RAM não é a causa deste erro.

Foi adicionado `partitions.csv` na pasta do sketch para os **16 MB de flash da ESP32-P4-Module-DEV-KIT**. Cada aplicativo OTA tem **6553600 bytes (6,25 MiB)**. Mantém NVS, otadata, dois aplicativos OTA, SPIFFS e coredump. A tabela coincide com default_16MB.csv do pacote Arduino ESP32 3.3.12. A PSRAM de 32 MB é memória de trabalho, não espaço para o firmware.

No Arduino IDE:

1. **Tools / Ferramentas → Flash Size → 16MB (128Mb)**.
2. **Partition Scheme → Custom**. O arquivo partitions.csv precisa permanecer junto do wifi_Eth_SDCard.ino. A opção Custom do pacote 3.3.12 elimina o antigo limite de 1.9 MB no verificador do IDE; o limite real continua sendo 6553600 bytes por aplicativo.
3. Mantenha **PSRAM habilitada** para a câmera.
4. Faça a primeira gravação dessa tabela pela **porta USB/serial**, não por OTA. A atualização de aplicativo pela rede não instala a nova tabela de partições. Depois disso, os próximos aplicativos podem voltar a ser enviados por OTA.

Não selecione Huge APP/No OTA, pois o projeto usa duas áreas de aplicativo. Não é necessário apagar toda a flash: deixe **Erase All Flash Before Sketch Upload → Disabled**, se disponível. NVS mantém endereço 0x9000 e tamanho 0x5000, permitindo preservar as preferências ao gravar sem apagar tudo. A localização dos aplicativos, SPIFFS e coredump muda em relação ao esquema antigo; dados antigos dessas regiões não são preservados por esta migração. Arquivos no cartão SD não dependem dessa tabela.

Validação desta revisão: **65 testes passaram** (60 de interface e 5 da tabela de partições), conferindo alinhamento, ausência de sobreposição, limite físico, dois slots OTA iguais e espaço para o tamanho de firmware informado. Não houve recompilação ou gravação da placa.


## Clima externo — 1.15.0

Temperatura e umidade de Porto Alegre vêm do [Open-Meteo](https://open-meteo.com/en/docs), nas coordenadas -30.03283, -51.23019, iguais às do M5NanoC6. São condições estimadas pelo serviço meteorológico; a temperatura interna da CPU permanece separada.

A primeira consulta aguarda internet por Wi-Fi ou Ethernet e relógio sincronizado por NTP. Atualiza a cada 10 minutos; em caso de falha, tenta novamente após 2 minutos. O cache expira em 30 minutos, considerando também o horário informado pela API; dados ausentes ou vencidos viram `NaN`. A consulta HTTPS roda em tarefa própria no core 0, com resposta limitada a 2048 bytes. `/metrics` lê somente o cache. Novas consultas são suspensas durante OTA; uma consulta já iniciada pode terminar.

Use `weather_status` pela Serial, UDP ou terminal web para ver rede, NTP, validade, temperatura, umidade, idade e último resultado HTTP. Instale **ArduinoJson 7.x** (revisado com os cabeçalhos 7.4.3); HTTPClient e NetworkClientSecure acompanham o pacote Arduino ESP32.

Firmware v1.20.0, based on the supplied Wi-Fi / Ethernet / SD project.
Hardware pins follow DeviceConfig.h; the navigation button now uses GPIO27. Use the original board,
ESP32 Arduino core and libraries configured for your ESP32-P4 and Wi-Fi companion.
This update does not configure the companion or change SDMMC wiring.

Documentação revisada em **22/10/2026**, para o firmware **1.20.0**.

Estado atual: botão **GPIO27/GND** para acordar e avançar; carinhas de robô em
toda a tela após **60 s** sem interação; bip nas trocas de página pelo ES8311.
Segurar o botão não apaga Wi-Fi. A web permanece na porta 80 e as métricas na
porta configurada. Consulte [comandos](COMANDOS.md), [guia em português](LEIA-ME.md)
e [Prometheus/Grafana](PROMETHEUS.md).

Validação mais recente: **69 testes passaram (interface e partições)**. Esta revisão não compila nem grava o firmware; botão, áudio e saída do protetor
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
Os 69 testes de interface e partições passaram na validação mais recente. A compilação será feita pelo usuário.

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

## Métricas UDP — 1.15.1

| Métrica | Significado |
| --- | --- |
| `esp_udp_listener_up` | Gauge: 1 se a última abertura da porta UDP4210 retornou sucesso; 0 antes da abertura ou após falha. Indica abertura local, não conectividade de ponta a ponta. |
| `esp_udp_packets_received_total` | Counter: datagramas identificados por parsePacket, incluindo comandos inválidos e maiores que o limite. Datagramas vazios não são detectáveis por esse retorno. |
| `esp_udp_replies_sent_total` | Counter: respostas UDP com todos os bytes escritos e endPacket bem-sucedido. Não confirma recebimento pelo destinatário. Cada chamada de resposta conta separadamente. |
| `esp_udp_send_errors_total` | Counter: tentativas de resposta sem destinatário válido, falha ao iniciar/concluir envio ou escrita incompleta. Uma falha conta uma vez por tentativa. |

Os três contadores reiniciam no boot, sem persistência NVS, e não são alterados por reset_counters. Respostas HTTP e Serial não os incrementam. Perder o link não fecha automaticamente o socket nem muda o resultado da última abertura.

No Grafana: `rate(esp_udp_packets_received_total[5m])` para pacotes/s e `increase(esp_udp_send_errors_total[1h])` para erros na última hora.

## MQTT e Home Assistant — 1.16.0

Abra **MQTT / Home Assistant** no menu web (`/mqtt`, porta 80), informe o endereço do broker sem prefixo mqtt://, porta (padrão 1883), usuário e senha e marque **Ativar MQTT**. O recurso inicia desativado. Configure a integração MQTT do Home Assistant com o mesmo broker; o firmware é cliente, não instala nem executa um broker.

A configuração fica no valor `config` do namespace Preferences `mqtt`, com verificação de escrita. A aplicação ocorre após aproximadamente um segundo. Senha vazia mantém a anterior; **Apagar senha salva** remove a senha. A senha não aparece na página, no JSON de consulta ou em mqtt_status. Transporte TCP sem TLS e armazenamento nas preferências da placa: utilize na rede local.

Usa ESP-MQTT já incluído no pacote Arduino ESP32 3.3.12, sem PubSubClient. A conexão/reconexão roda na tarefa interna do cliente; publicações comuns usam enqueue, com outbox limitado a 16 KiB. Ao alterar/desativar a configuração, o encerramento da conexão anterior pode esperar uma operação de rede por até o timeout de 5 s. Não há compilação ou gravação automática.

### Entidades e tópicos

Descoberta retida sob `homeassistant/<componente>/<id>/<entidade>/config`, com identificação derivada do MAC de fábrica. O tópico base é `esp32/esp32p4_<id>`; confira o valor exato na página ou em `mqtt_status`. Cada placa possui um ID próprio. Descoberta é reenviada na reconexão e quando Home Assistant publica online em `homeassistant/status`. Configure o prefixo de descoberta padrão homeassistant.

- Sensores: temperatura CPU, temperatura e umidade externas de Porto Alegre (Open-Meteo), uptime, heap livre, RSSI Wi-Fi, SD livre e usado. Valores indisponíveis são null no JSON, apresentados como desconhecidos.
- Estado binário do SD, interruptor do LED e cinco botões: próxima tela, telas automáticas, ligar/desligar carinhas e testar som.
- Evento **Botao GPIO27** com tipo `press`: uma publicação por pressão após debounce, preservando a navegação no OLED. Eventos não são retidos nem armazenados enquanto offline.
- `<base>/state`: JSON retido a cada 30 s e após comandos. Sensores numéricos expiram após 120 s sem atualização.
- `<base>/availability`: online/offline retido, com Last Will QoS 1 e keepalive 30 s. Queda inesperada é detectada pelo broker, portanto não é instantânea.
- `<base>/command`: aceita somente `led_on`, `led_off`, `tela:next`, `tela:auto`, `protetor:on`, `protetor:off` e `audio_test`. Envie sem retain. Comandos retidos, fragmentados, maiores que 31 bytes ou fora da lista são ignorados. A fila comporta oito comandos. Não expõe reinício, exclusão de arquivos ou credenciais.

Durante OTA/reinício agendado, publicações periódicas e execução de comandos ficam suspensas; comandos recebidos são descartados. A conexão interna MQTT pode permanecer ativa. Não há histórico offline de amostras: após reconectar publica o estado atual. Desativar MQTT mantém as entidades descobertas no Home Assistant, indisponíveis após o aviso offline ou expiração dos sensores; para removê-las, exclua as configurações retidas correspondentes no broker.

### API e diagnóstico

`GET /api/mqtt/config` retorna enabled, host, port, user, password_set, connected, base_topic e contadores, nunca a senha. `POST /api/mqtt/config` recebe formulário enabled (0/1), host, port, user, password opcional e clear_password (1 para apagar), com header `X-ESP32-Command: 1`. Respostas: 400 inválido, 409 durante OTA/reinício/aplicação anterior, 503 falha NVS. As rotas retornam no-store e não habilitam CORS.

O comando `mqtt_status` funciona na Serial, UDP e terminal web. No Prometheus, `mqtt_connected` indica a conexão; `mqtt_messages_sent` conta publicações QoS 1 confirmadas pelo broker (inclui descoberta/estado/eventos), `mqtt_messages_received` conta mensagens completas/iniciadas recebidas incluindo anúncios do Home Assistant e comandos ignorados; `mqtt_publish_errors` conta falhas locais, conexão, assinatura, fila e descarte de publicações pelo cliente. Os três são counters e reiniciam no boot, sem persistência e sem alteração por reset_counters. `esp_metric_available{group="mqtt"}=1` indica suporte implementado, mesmo desativado.

Validação desta revisão: **44 testes da interface passaram**; não houve compilação, conexão real ao broker ou teste físico. Após compilar/gravar, salve o broker, confirme connected em mqtt_status e confira o dispositivo ESP32-P4 na integração MQTT do Home Assistant. Teste o LED, os controles de tela e o evento físico GPIO27. Referência: [MQTT no Home Assistant](https://www.home-assistant.io/integrations/mqtt).

## Processamento local (edge computing) — 1.17.0

Abra **Processamento local** no menu web (`/edge`, porta 80), ajuste os limites e marque **Ativar regras locais**. O recurso inicia desativado e o som também. As decisões usam dados locais da placa e não dependem de internet, NTP, MQTT ou Prometheus. É um motor de regras determinístico, sem modelo de IA.

| Regra | Condição inicial | Recuperação |
| --- | --- | --- |
| `cpu_hot` | CPU maior ou igual a 70 °C | CPU menor ou igual ao limite menos 3 °C |
| `heap_low` | Heap livre abaixo de 65536 bytes | Heap maior ou igual ao limite mais 8192 bytes |
| `network_down` | Sem Wi-Fi STA e sem Ethernet com IP | Uma das duas redes volta; não testa acesso à internet |
| `udp_errors` | Pelo menos 5 falhas de envio UDP nos últimos 60 s | Quantidade na janela cai abaixo do limite |

Amostragem uma vez por segundo no ciclo da aplicação, sem espera ativa. Rede e UDP aguardam 60 s após boot/ativação/retorno de OTA para formar a janela inicial. A janela UDP usa 60 posições por segundo e o contador esp_udp_send_errors_total; não detecta por si só perda de pacotes ou ausência de respostas remotas. Uma pausa de amostragem de 60 s descarta a janela anterior e recomeça o aquecimento.

**Confirmação padrão: 10 s** contínuos, tanto para ativar quanto para recuperar. Oscilações ou dado inválido cancelam a confirmação pendente. CPU inválida não cria um zero e não encerra um alerta existente; a regra fica indisponível até nova leitura válida. **Intervalo mínimo: 300 s** entre notificações de alerta da mesma regra; condições persistentes repetem a notificação após esse prazo. Recuperações são notificadas após confirmação, sem esperar o intervalo. O estado ativo pode mudar mesmo quando uma nova notificação está limitada pelo intervalo.

### Ações e histórico

As notificações e recuperações aparecem na Serial e em `/log.txt` quando o SD aceita a escrita. A página mostra os últimos **16 eventos em RAM**, com sequência e uptime em segundos. Um reinício limpa esse histórico e os contadores, mas não apaga os logs já gravados. Cada regra pode produzir alerta repetido no intervalo configurado. Se **Emitir som nos alertas** estiver marcado, solicita o tom de teste ao sistema de áudio existente; indisponibilidade ou fila ocupada pode impedir o som sem impedir o registro. Não reinicia a placa, não apaga preferências e não altera o LED ou a navegação automaticamente.

Durante OTA ou reinício agendado, a avaliação e suas ações ficam pausadas. Ao retomar, confirmações e janela UDP são reiniciadas; os estados ativos anteriores permanecem até reavaliação válida. Salvar configuração reinicia a janela e limpa estados ativos/pendentes sem criar eventos de recuperação; mantém contadores e histórico do boot. A configuração é salva em um valor `config` no namespace Preferences `edge`, com conferência da escrita.

### MQTT / Home Assistant

Se MQTT estiver conectado, eventos são enviados sem retain em `<base>/edge/event`: JSON com rule, active, sequence e uptime. Não há fila histórica de eventos gerados offline; o estado atual segue no JSON `<base>/state` (edge_alert ON/OFF, edge_enabled e edge_active_count), publicado no máximo a cada 30 s e solicitado imediatamente após uma notificação. O Home Assistant descobre o sensor binário **Alerta local** no mesmo dispositivo MQTT. Regras continuam funcionando com o broker desligado.

### API, diagnóstico e métricas

- `GET /api/edge/config`: enabled, sound, cpu_limit, heap_limit, udp_limit, confirm_seconds, cooldown_seconds.
- `POST /api/edge/config`: os mesmos campos como formulário; enabled/sound são 0 ou 1 e é obrigatório o header `X-ESP32-Command: 1`. Limites: CPU 20–100 °C; heap 8192–1048576 bytes; UDP 1–10000 falhas/60 s; confirmação 1–600 s; intervalo 30–86400 s. Retorna 400 inválido, 409 OTA/reinício ou 503 NVS. Rotas sem cache e sem CORS.
- `GET /api/edge/status` e comando `edge_status` (Serial, UDP e terminal web): configuração, amostras locais, pausa, estado/validade/confirmação de cada regra e histórico.
- Prometheus: `esp_edge_enabled`, `esp_edge_active_alerts`, `esp_edge_events_total`, `esp_edge_rule_active{rule="..."}`, `esp_edge_rule_available{rule="..."}`, `esp_edge_rule_alerts_total{rule="..."}`. Eventos contam alertas/repetições e recuperações; o contador por regra conta somente notificações de alerta. Contadores reiniciam no boot e não são alterados por reset_counters. Disponibilidade 0 significa regra desativada, em aquecimento ou sem leitura válida. Estado ativo anterior pode permanecer com disponibilidade 0.

Validação desta revisão: **53 testes da interface passaram**. Incluem configuração, limites, falhas de persistência simuladas e atualização do estado sem sobrescrever edições. Não houve compilação, teste do motor C++ em execução ou teste físico. Após compilar/gravar, ative as regras pela web, confira edge_status e valide alertas/recuperações controlados na placa.

## Câmera OV5647 MIPI-CSI — 1.18.0

A câmera confirmada para esta configuração é **OV5647 no conector MIPI-CSI com cabo flat**. O firmware tenta inicializá-la automaticamente no boot. Abra **Câmera** no menu web (`/camera`, porta 80). A página consulta uma imagem JPEG por segundo; o botão Pausar suspende somente a visualização, não a captura na placa. A imagem anterior recebe aviso de desatualização se a consulta falhar.

### Captura e integração

- Usa ESP_Video e esp_video já presentes no Arduino ESP32 3.3.12, com driver OV5647/ISP e encoder JPEG do ESP32-P4. Não usa o esp_camera dos módulos ESP32-CAM DVP.
- Compartilha o handle I²C da Wire no barramento 0 (SDA7/SCL8) com OLED e ES8311. Não cria outro barramento nem altera os pinos do botão/áudio. O sensor usa SCCB a 100 kHz.
- Captura RGB565 com dois buffers; converte em tarefa própria no core 0 para JPEG de qualidade 70, limitado a duas codificações por segundo. A resolução vem do driver instalado (OV5647 padrão 800×800 neste pacote); aceita até 1280×1280, com dimensões múltiplas de 16.
- Requer PSRAM habilitada. Reserva dois buffers de saída JPEG de aproximadamente largura×altura×3+1024 bytes cada, além dos buffers do driver de vídeo. Falha de inicialização libera os recursos da câmera; o restante da aplicação continua. Não há repetição automática da inicialização: confira camera_status e reinicie após corrigir a causa.
- O servidor copia o JPEG mais recente com proteção de acesso; não captura/espera pelo sensor dentro da requisição web. Falta de memória, cache ocupado ou imagem com mais de 3 s retorna HTTP 503. A transferência HTTP do JPEG ainda ocupa o handler durante o envio.
- OTA pausa novas capturas/codificações do worker e bloqueia snapshots; uma operação já iniciada pode terminar. O pipeline CSI/ISP pode permanecer ativo. Câmera sem responder pode deixar somente o worker aguardando frame, sem esperar no ciclo da web.

### Rotas, diagnóstico e métricas

`GET /camera.jpg`: JPEG atual sem cache, para abrir no navegador ou usar como URL de imagem em outra integração. Não é fluxo MJPEG, RTSP ou gravação de vídeo. `GET /api/camera/status` e comando `camera_status` (Serial, UDP e terminal web) mostram inicialização, disponibilidade, pausa, resolução, quantidade de JPEGs, falhas, idade da última imagem e diagnóstico de captura. `ready` indica inicialização concluída; `available` exige também JPEG recente e ausência de pausa.

Prometheus inclui `camera_initialized`, `camera_up`, `camera_frames_total`, `camera_errors_total` e `camera_frame_age_seconds`. Frames contam JPEGs disponibilizados no cache, não todos os frames do sensor. Falhas contam inicialização, frame inválido/incompleto e erro/timeout de codificação. Idade é NaN antes do primeiro JPEG. Contadores reiniciam no boot. Imagens não são publicadas por MQTT nem gravadas automaticamente no SD; a integração MQTT anterior continua ativa.

Validação desta revisão: **60 testes da interface passaram**, incluindo atualização, pausa, erro JPEG, descarte de URLs antigas e bloqueio de consultas sobrepostas. Não houve compilação nem teste físico da câmera. Após compilar/gravar, abra /camera e confira camera_status; se não houver imagem, verifique a detecção do sensor, cabo CSI e alimentação. Referência da placa: [Waveshare ESP32-P4-Module-DEV-KIT](https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT).

Diagnóstico MQTT em 1.19.1: mqtt_status e GET /api/mqtt/config incluem disconnects, last_error_type, broker_return_code, socket_errno, transport_error, heap_free e network_up. O tipo 1 identifica transporte TCP; tipo 2 indica recusa do broker (código 4: credenciais inválidas, 5: acesso não autorizado). Os erros de conexão são limpos após uma reconexão bem-sucedida; o contador errors continua acumulado e inclui outras falhas locais. Um contador errors alto, isoladamente, não comprova senha incorreta ou falta de sockets. O usuário confirmou enabled=true, connected=false, sent=204 e errors=575 na versão anterior; isso confirma comunicação anterior e desconexão atual, sem identificar a causa final.
