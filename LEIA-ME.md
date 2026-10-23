# ESP32-P4 — Monitor de rede com OLED

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

Versão 1.20.0 do projeto Wi-Fi, Ethernet e cartão SD. Inclui Prometheus, página de log, seis telas OLED,
crash_info, monitoramento de rede, dois cores, serial, terminal web e OTA.

Documentação revisada em **22/10/2026**, conforme o firmware **1.20.0**.
Referências: [comandos](COMANDOS.md), [Prometheus/Grafana](PROMETHEUS.md)
e [histórico de alterações](CHANGELOG.md).

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

## Métricas para Prometheus

A temperatura interna da CPU é publicada em `esp_cpu_temperature_celsius`.
No Grafana, consulte essa métrica e use a unidade Celsius (°C).
O comando `temp` usa o mesmo sensor. Falhas são sinalizadas sem simular leituras.

A conexão segue o M5NanoC6. Em /prometheus, informe o endereço do servidor,
a porta de métricas da placa e o intervalo de coleta (5–3600 segundos).
Salve nas Preferences; a porta muda cerca de um segundo depois, sem reiniciar.
A interface web e a API continuam na porta 80, sem redirecionamento.

O padrão inicial é /metrics na porta 80, compartilhando a web. Escolha outra
porta para separar a coleta; 3232 fica reservada ao OTA. A coleta permanece ativa,
sem chave de ativação ou campo separado para porta do servidor.
Endereço e porta da versão anterior são preservados; o intervalo inicia em 15 s.

Copie o YAML exibido ou baixe /prometheus.yml e aplique no servidor Prometheus.
O intervalo salvo é escrito nesse YAML; só passa a valer no servidor após recarregá-lo.
Use IP fixo/reserva DHCP e a série up para alertar quando a placa não responder.
Todas as métricas anteriores, inclusive SD, permanecem disponíveis.

O catálogo e a API estão em [PROMETHEUS.md](PROMETHEUS.md).
Os 69 testes de interface e partições passaram na validação mais recente; a compilação será feita pelo usuário.
Coleta e persistência reais ainda precisam de teste na placa.

## Leitura do log pela página web

No menu, abra **Log do dispositivo**, ou acesse `/logs` no endereço IP da placa.
A página lê `/log.txt` do cartão SD. Escolha 25, 100 ou 250 linhas e clique em
**Atualizar agora**. A opção automática consulta a cada cinco segundos e pausa
quando a aba está oculta. A busca filtra apenas o trecho carregado; desmarque
**Acompanhar o final** para ler sem rolar automaticamente.

São lidos no máximo os últimos 8 KB, sem percorrer o arquivo inteiro. Um aviso
indica quando o trecho foi limitado. As consultas não aumentam o arquivo de log
nem acordam o OLED. Falhas de cartão, arquivo ausente e OTA em andamento aparecem
na página; leituras antigas permanecem visíveis com aviso de desatualização.

## Botão, protetor e áudio — 1.14.0

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

Os 35 testes da interface passaram. Sem compilação ou teste físico nesta revisão.

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

## Dois cores e atualização de firmware

O core 0 recebe a serial e coloca comandos em uma fila de oito posições. O core 1
executa comandos e cuida de web, UDP, SD, OLED e OTA. Não há acesso simultâneo ao
SD/OLED pelas duas tarefas. Use `cores` para conferir o core de execução e a fila.
Se a fila encher, comandos inteiros são descartados, com aviso na serial.
A tarefa de áudio `page_audio` também roda no core 0, recebendo notificações e
transmitindo o bip por I2S. Ela não acessa o OLED nem faz operações I²C; a configuração
do codec acontece no início, usando o mesmo `Wire` do display.

Para atualizar pelo navegador, abra `/firmware`, entre como `admin` e use a senha
fixa `123456`. Ela também é usada pelo Arduino IDE, sem geração ou leitura nas
Preferences; senhas antigas salvas são ignoradas. Use somente em rede confiável.
Envie `ota_password` pela serial para consultá-la novamente; esse comando não
revela a senha pelo navegador ou UDP.

Para atualizar pelo Arduino IDE, selecione a porta de rede `esp32-p4-node` e use
a mesma senha. O serviço usa a porta 3232 e descoberta mDNS na rede local.
`ota_info` informa o estado do serviço e o tamanho disponível na partição OTA.

Instale esta versão primeiro por **USB** com uma tabela que tenha `otadata` e dois
slots de aplicativo. Depois envie o `.bin` do aplicativo, compilado para a mesma
placa/configuração, pela rede. Não envie bootloader, partições ou imagem merged.
A tabela padrão de 4 MB oferece 1.310.720 bytes por aplicativo; se necessário,
escolha outra tabela que caiba na flash real da placa. OTA não muda as partições.

O navegador mostra o progresso. O firmware exige autenticação, limita o tamanho,
confere o fim da transferência e só ativa a imagem depois de verificá-la.
Transferências incompletas não substituem a partição em execução. Durante OTA,
a execução dos comandos fica suspensa; a recepção serial continua com fila limitada.
Mantenha a alimentação. Use em rede local confiável. Não há rollback automático
se uma nova versão válida iniciar e depois falhar em execução.

Última compilação concluída registrada: a versão anterior **1.11.0** compilou para `ESP32P4 Dev Module` com Arduino ESP32
**3.3.12**, usando **1.223.348 bytes (93%)** da partição padrão de 1.310.720 bytes.
Os 22 testes automatizados das interfaces web passaram, incluindo log, `crash_info` e controle de telas.
A compilação usou as opções padrão; na instalação, escolha a revisão do chip,
USB e flash reais da placa. Ainda não foram feitos testes físicos de `crash_info`
e da alternância contínua de telas, nem gravação desta versão na placa.

## Painel e comandos

Abra `/info` no endereço IP da placa. Os cartões atualizam a cada cinco segundos
sem recarregar a página. Use o botão para pausar ou retomar. O terminal abaixo dos
cartões aceita os mesmos comandos do UDP, com atalhos para diagnóstico, LED, OLED
e arquivos. As setas para cima/baixo percorrem o histórico. Senhas de `wifi_add`
não aparecem na saída nem no histórico do terminal.

No Monitor Serial, selecione **115200 baud** e final de linha **LF, CR ou CRLF**.
Depois da inicialização, experimente `help`, `status`, `led_on` ou `sd_list`.
Para USB nativo, habilite **USB CDC On Boot** conforme a configuração da placa;
quando desabilitado, `Serial` mantém a UART configurada pelo Arduino.

O limite é 254 bytes por comando. A serial descarta linhas grandes por inteiro.
Respostas web têm limite de 8 KB, com aviso de truncamento: para arquivos grandes,
use `sd_tail`. A página pede confirmação antes de alterar arquivos/configurações.
Se perder a resposta, confira o estado antes de repetir operações de escrita.
As respostas voltam ao canal de origem; logs de diagnóstico continuam na serial.
Os reinícios por comando aguardam 1,5 segundo para entregar a resposta.

Os scripts e estilos são locais e não dependem de acesso à Internet. O portal
continua destinado a uma rede local confiável. Consulte `COMANDOS.md` para a lista
completa de comandos. Leituras extensas no cartão SD ainda podem atrasar o painel.

## Hardware

| Recurso | Configuração do projeto |
|---|---|
| Processador | ESP32-P4 com suporte Wi-Fi da placa/companion configurado no Arduino |
| Display | OLED SSD1306, 128 × 64, endereço I2C 0x3C |
| OLED SDA / SCL | GPIO 7 / GPIO 8 |
| LED | GPIO 1 |
| Botão de navegação | GPIO27, ligado ao GND, INPUT_PULLUP |
| Codec de áudio | ES8311, I²C 0x18, compartilhando SDA7/SCL8 com o OLED |
| I2S MCLK / BCLK / LRCK / DOUT | GPIO13 / GPIO12 / GPIO10 / GPIO9 |
| Amplificador do alto-falante | GPIO53, habilitação em HIGH durante o bip |
| Ethernet | PHY IP101, endereço 1 |
| Ethernet MDC / MDIO | GPIO 31 / GPIO 52 |
| Alimentação PHY | GPIO 51 |
| Clock Ethernet | EMAC_CLK_EXT_IN |
| Cartão SD | SD_MMC, mantendo a configuração original da placa |

O OLED é monocromático. Os pinos estão em `DeviceConfig.h`. O pacote mantém o
hardware fornecido; não configura a ligação do chip companion Wi-Fi nem altera
a fiação SDMMC. Use a mesma seleção de placa, core ESP32 e configuração Wi-Fi
companion empregadas no projeto original.

## Instalação

1. Extraia o arquivo ZIP.
2. Abra `wifi_Eth_SDCard/wifi_Eth_SDCard.ino` no Arduino IDE.
3. Mantenha os arquivos `.h` e `.cpp` juntos na pasta do sketch, incluindo
   `NetworkMonitor.h` e `NetworkMonitor.cpp`.
4. Utilize as bibliotecas originais, incluindo Adafruit SSD1306 e Adafruit GFX.
   O ping utiliza a API nativa do ESP-IDF; não exige biblioteca de ping adicional.
5. Compile e grave usando a configuração adequada à sua placa.

As configurações de rede ficam em Preferences. Sem conexão Wi-Fi, o projeto abre
`ESP32_P4_CONFIG`; acesse `http://192.168.4.1/wifi` para configurar redes.
O Ethernet é inicializado mesmo quando nenhuma rede Wi-Fi salva conecta.

## Telas

| Número | Conteúdo |
|---|---|
| 0 | Relógio, data e estado Wi-Fi/Ethernet |
| 1 | SSID, IP, gateway, RSSI, canal e barras de sinal Wi-Fi |
| 2 | Ethernet, IP, gateway, quedas e tempo desconectado |
| 3 | Processador, memória livre/mínima, estado do SD e tempo ligado |
| 4 | Ping ao gateway por interface, percentual sem resposta e gráfico |
| 5 | Quatro eventos de conexão mais recentes |

As páginas de informação mudam automaticamente a cada oito segundos e atualizam
uma vez por segundo, enquanto o protetor está inativo. `tela:N` fixa uma tela;
`tela:next` avança; `tela:auto` retoma a troca
automática. A moldura é composta no buffer do OLED antes de ser enviada.
Comandos mostram o terminal por cinco segundos. Eventos de rede não substituem
as carinhas enquanto o protetor está ativo. Durante operações com progresso,
o protetor é interrompido e o terminal permanece visível até a operação terminar.

## Botão

- Pressione: acorda e avança uma tela, mantendo a troca automática.
- Segure: permanece na mesma ação, sem repetir e sem apagar as redes.

O botão fica entre GPIO27 e GND. RST é independente. Para limpar as redes,
use reset_wifi no terminal. Consulte a seção 1.14.0 para protetor e áudio.

## Monitor de rede

O monitor mede o tempo de ida e volta de um ping ICMP ao gateway de cada interface.
Inicia uma medição a cada aproximadamente cinco segundos. Com Wi-Fi e Ethernet
conectados, alterna as interfaces: cada uma recebe uma medição a cada dez segundos.
Com apenas uma conectada, mede essa interface a cada cinco segundos. Cada ping é
vinculado à interface correspondente e tem timeout de um segundo, em tarefa
separada do loop principal.

O gráfico guarda 24 resultados por interface. As barras representam o tempo em
milissegundos com escala adaptativa; um X indica timeout. Na tela 4, o gráfico
alterna entre Wi-Fi e Ethernet a cada oito segundos. `net_monitor` retorna as
estatísticas das duas interfaces e o histórico por UDP.

Quedas significam transições observadas de conectado para desconectado. O tempo
offline é acumulado separadamente para cada interface. Os quatro últimos eventos
mostram segundos desde a inicialização. Tudo fica em RAM e é zerado no reboot.
Operações bloqueantes já existentes, como LED, leitura SD e inicialização, podem
atrasar a observação das mudanças de conexão.

Sem resposta ao ping pode significar bloqueio de ICMP no roteador. Não equivale
necessariamente a queda de rede e não verifica acesso à Internet.

## Comunicação e comandos

- UDP: porta **4210**, resposta ao endereço e porta do remetente.
- HTTP: `/` página inicial, `/info` informações, `/wifi` configuração de redes.
- `help`: comandos disponíveis.
- Lista detalhada: consulte `COMANDOS.md`.

Comandos UDP acima de 254 bytes são rejeitados para evitar estouro do buffer.
Os nomes e aliases anteriores continuam disponíveis. As prioridades de rota
configuradas no código são Wi-Fi 50 e Ethernet 10; o ping força a interface de
medição para distinguir os links.

## Alterações de estabilidade incluídas

- Correção do espaço para terminador na recepção UDP.
- Eventos de rede passam por uma fila e atualizam o OLED no loop principal.
- Ethernet inicializa independentemente do sucesso da conexão Wi-Fi.
- UDP habilitado também em modo AP.
- Configuração de NTP carregada em todo boot, permitindo sincronização por Ethernet.

## Validação

Foram revisados estrutura dos arquivos, includes locais, preservação dos comandos
e pinos, integração das telas, limite UDP e integridade do ZIP. Consulte as notas
da versão atual para compilação e testes de interface. Os testes físicos na placa
ainda precisam ser realizados.

Teste as seis telas, Wi-Fi e Ethernet juntos e separados, retirada do cabo,
perda de Wi-Fi, AP sem credenciais, comandos SD e toque/pressão longa do botão.

Referências: [ICMP Echo da Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/icmp_echo.html)
e [ESP-NETIF](https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/api-reference/network/esp_netif.html).

## Melhorias da versão 1.5.1

Histórico: a ação antiga de limpar redes ao segurar o botão não existe no GPIO27
da versão atual. Consulte a seção **Botão, protetor e áudio — 1.14.0**.

- `net_monitor` mostra RTT mínimo, médio e máximo de respostas válidas desde o
  boot e idade da última medição. Timeouts não entram na média.
- `/info` apresenta o mesmo relatório de rede, atualizado com a página a cada
  dez segundos. A tela de ping indica a idade da medição do gráfico selecionado.
- `led_blink[:ms]` mantém a duração padrão de 3000 ms, limite de 10000 ms e troca
  a cada 200 ms, agora sem bloquear o loop. `led_on` e `led_off` cancelam a
  sequência imediatamente; um novo `led_blink` reinicia a duração.
- Segurar o botão por cinco segundos arma a limpeza e mostra uma mensagem; a
  rede continua sendo atendida enquanto ele está pressionado. Soltar limpa as
  redes e reinicia. O procedimento final de reinício mantém a indicação do LED.

## Portal moderno — versão 1.6.0

Acesse `/info` no IP do dispositivo para ver Wi-Fi, Ethernet, memória, tempo ligado,
SD, redes salvas, estatísticas de ping e histórico. Atualiza a cada dez segundos.
Acesse `/wifi` para editar os cinco perfis. As páginas são responsivas, funcionam
sem Internet e não carregam fontes, estilos ou scripts externos.

- Senha vazia mantém a senha atual apenas quando o SSID é o mesmo.
- Para uma nova rede, informe a senha ou marque **Rede aberta**.
- Para transformar uma rede existente em rede sem senha, marque **Rede aberta**.
- Para remover um perfil, apague o SSID.
- **Mostrar/Ocultar** atua só sobre a senha que você digitou. Senhas já gravadas
  nunca são enviadas ao navegador.
- SSID: até 32 bytes; senha: de 8 a 63 bytes ou 64 caracteres hexadecimais.

O servidor valida os cinco perfis antes de gravar. Confirma os valores por leitura
após a gravação e só então agenda o reboot, cerca de 1,5 segundo após a resposta.
Se a gravação falhar, tenta restaurar os perfis anteriores e informa o resultado;
não agenda o reboot em caso de erro. A gravação de vários campos não é uma
transação atômica: uma interrupção de energia no meio pode deixar dados parciais.
Os comandos UDP existentes permanecem disponíveis.

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
