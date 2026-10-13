# ESP32-P4 — Monitor de rede com OLED

Versão 1.11.0 do projeto Wi-Fi, Ethernet e cartão SD. Inclui página de log, seis telas OLED,
proteção de tela, monitoramento de rede, dois cores, serial, terminal web e OTA.

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

## Proteção de tela

Na Waveshare ESP32-P4-WIFI6, os dois botões da placa são **BOOT** e **RST**.
O RST reinicia o microcontrolador por hardware; não pode ser usado como botão
de despertar por software. O GPIO 2 do projeto espera um **botão externo ligado
entre GPIO 2 e GND**, com `INPUT_PULLUP`. O BOOT usa GPIO 35, compartilhado com
Ethernet TXD1; não foi remapeado para não prejudicar a rede.
Use **Acordar tela** no terminal web ou `protetor:wake` pela serial/UDP.
O comando `botao_status` mostra o GPIO configurado e sua leitura: HIGH solto,
LOW pressionado. Se ela não mudar ao apertar o botão externo, confira a ligação.
Referência da placa: https://docs.waveshare.com/ESP32-P4-WIFI6.

Após **60 segundos sem interação**, o OLED reduz o brilho e mostra um relógio
que se desloca pela tela. Após **mais 4 minutos**, o controlador apaga o OLED.
O ESP32 continua atendendo a rede, a serial e os demais serviços.

O primeiro toque curto no botão acorda a tela mantendo a página. O próximo toque
troca a página normalmente. Comandos usuais e eventos de conexão que aparecem no
OLED também acordam o display. A rotação das páginas e a atualização automática
do painel web não reiniciam o contador. Transferências e OTA mantêm o progresso
visível. O pressionamento longo de cinco segundos continua limpando o Wi-Fi.

Pela serial, UDP ou terminal web, use `protetor:on` para iniciar agora,
`protetor:wake` para acordar, `protetor:off` para desabilitar a ativação automática
e `protetor:auto` para habilitá-la. `protetor:tempo:120` altera a espera para dois
minutos; são aceitos inteiros de **15 a 3600 segundos**. `protetor:status` consulta
o estado sem acordar a tela. As configurações são temporárias: ao reiniciar,
volta o padrão automático de 60 segundos. A proteção manual não altera a opção
de ativação automática. Nenhum acesso ao OLED foi transferido para o core 0.

Teste na placa: aguarde 60 segundos para conferir relógio e brilho; aguarde mais
quatro minutos para apagar; pressione o botão uma vez para acordar sem mudar a
página; confira os comandos e o progresso durante uma transferência/OTA.

## Dois cores e atualização de firmware

O core 0 recebe a serial e coloca comandos em uma fila de oito posições. O core 1
executa comandos e cuida de web, UDP, SD, OLED e OTA. Não há acesso simultâneo ao
SD/OLED pelas duas tarefas. Use `cores` para conferir o core de execução e a fila.
Se a fila encher, comandos inteiros são descartados, com aviso na serial.

Para atualizar pelo navegador, abra `/firmware`, entre como `admin` e use a senha
indicada na serial por `[OTA]`. Ela é gerada por dispositivo e permanece salva.
Envie `ota_password` pela serial para consultá-la novamente; esse comando não
revela a senha pelo navegador ou UDP. Se não for possível guardar/verificar a
senha, OTA fica desabilitado.

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

Validação: a versão **1.11.0** compilou para `ESP32P4 Dev Module` com Arduino ESP32
**3.3.12**, usando **1.223.348 bytes (93%)** da partição padrão de 1.310.720 bytes.
Os 20 testes automatizados das interfaces web passaram, incluindo log e despertar.
A compilação usou as opções padrão; na instalação, escolha a revisão do chip,
USB e flash reais da placa. Ainda não foram feitos testes físicos da proteção
de tela nem gravação desta versão na placa.

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
| Botão de navegação e limpeza | GPIO 2, ligado ao GND, INPUT_PULLUP |
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

As telas mudam automaticamente a cada oito segundos e atualizam uma vez por
segundo. `tela:N` fixa uma tela; `tela:next` avança; `tela:auto` retoma a troca
automática. A moldura é composta no buffer do OLED antes de ser enviada.
Comandos e eventos podem mostrar o terminal por cinco segundos. Durante operações
com progresso, o terminal permanece visível até a operação terminar.

## Botão

- Toque curto e solte: avança uma tela e desativa a troca automática.
- Segure por cinco segundos e solte: apaga as redes Wi-Fi salvas e reinicia.

O botão é GPIO 2, independente do botão EN da placa. A limpeza mantém firmware,
configurações de horário e arquivos do SD. `reset_wifi` continua disponível.

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
