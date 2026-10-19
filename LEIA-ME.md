# ESP32-P4 — Monitor de rede com OLED


## Clima externo — 1.15.0

Temperatura e umidade de Porto Alegre vêm do [Open-Meteo](https://open-meteo.com/en/docs), nas coordenadas -30.03283, -51.23019, iguais às do M5NanoC6. São condições estimadas pelo serviço meteorológico; a temperatura interna da CPU permanece separada.

A primeira consulta aguarda internet por Wi-Fi ou Ethernet e relógio sincronizado por NTP. Atualiza a cada 10 minutos; em caso de falha, tenta novamente após 2 minutos. O cache expira em 30 minutos, considerando também o horário informado pela API; dados ausentes ou vencidos viram `NaN`. A consulta HTTPS roda em tarefa própria no core 0, com resposta limitada a 2048 bytes. `/metrics` lê somente o cache. Novas consultas são suspensas durante OTA; uma consulta já iniciada pode terminar.

Use `weather_status` pela Serial, UDP ou terminal web para ver rede, NTP, validade, temperatura, umidade, idade e último resultado HTTP. Instale **ArduinoJson 7.x** (revisado com os cabeçalhos 7.4.3); HTTPClient e NetworkClientSecure acompanham o pacote Arduino ESP32.

Versão 1.15.0 do projeto Wi-Fi, Ethernet e cartão SD. Inclui Prometheus, página de log, seis telas OLED,
crash_info, monitoramento de rede, dois cores, serial, terminal web e OTA.

Documentação revisada em **19/10/2026**, conforme o firmware **1.15.0**.
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
Os 36 testes de interface passaram na validação mais recente; a compilação será feita pelo usuário.
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
