# Comandos — ESP32-P4 v1.11.0

Envie por UDP à porta **4210**, pela serial a **115200 baud** ou pelo terminal
web em `/info`. O firmware responde pelo canal de origem. Na serial, termine
cada comando com LF, CR ou CRLF e aguarde o fim da inicialização.
Limite: **254 bytes por comando**. Nomes não distinguem maiúsculas/minúsculas;
argumentos preservam a caixa. Arquivos SD podem gerar várias respostas UDP.

## Proteção do OLED

| Comando | Função |
|---|---|
| `protetor:on` | Inicia a proteção imediatamente sem alterar a opção automática. |
| `protetor:off` | Desabilita a proteção automática e acorda a tela. |
| `protetor:auto` | Habilita a proteção automática com o tempo atualmente configurado. |
| `protetor:wake` | Acorda o OLED e reinicia a contagem de inatividade. |
| `protetor:status` | Consulta o estado sem acordar o display. |
| `protetor:tempo:120` | Ajusta a espera para 120 segundos e habilita a proteção automática. |
| `botao_status` | Mostra o GPIO configurado e sua leitura LOW/HIGH para diagnóstico do botão. |

Tempo aceito: inteiro de 15 a 3600 segundos. Padrão ao reiniciar: 60 segundos.
Após a ativação, o relógio se move com brilho reduzido por quatro minutos; depois
o OLED apaga. As opções não são gravadas em memória permanente. O primeiro toque
curto no botão acorda sem mudar a página; um segundo toque avança normalmente.

## Sistema

| Comando | Função | Exemplo |
|---|---|---|
| `help` | Lista comandos e aliases. | `help` |
| `info` | Informações gerais da placa. | `info` |
| `status` | Resumo de Wi-Fi, SD, relógio e heap. | `status` |
| `version` | Versão do firmware. | `version` |
| `build` | Data e hora da compilação. | `build` |
| `reason` | Motivo do último reset. | `reason` |
| `alive` | Responde ALIVE. | `alive` |
| `cores` | Core de execução, tarefa serial e ocupação da fila. | `cores` |
| `ota_info` | Serviço OTA, porta e tamanho da partição. | `ota_info` |
| `ota_password` | Mostra usuário/senha OTA somente na serial local. | `ota_password` |
| `uptime` | Tempo ligado em milissegundos. | `uptime` |
| `lastcmd` | Último comando; credenciais ficam ocultas. | `lastcmd` |
| `cmdcount` | Total de comandos, exceto lastcmd/cmdcount. | `cmdcount` |
| `reboot` | Reinicia após responder. | `reboot` |

## Diagnóstico e hardware

| Comando | Função | Exemplo |
|---|---|---|
| `health` | Diagnóstico de rede, OLED, SD montado, relógio e memória. Somente leitura. | `health` |
| `selftest` | Mesmo diagnóstico de health; não realiza teste de escrita ou teste elétrico. | `selftest` |
| `temp` | Temperatura interna do processador. | `temp` |
| `cpu` | Modelo, revisão e frequência. | `cpu` |
| `chip_info` | Modelo, revisão, núcleos, frequência e SDK. | `chip_info` |
| `ram` | Heap livre, mínimo e maior bloco disponível. | `ram` |
| `heap` | Heap livre em bytes. | `heap` |
| `heap_min` | Menor heap livre desde o boot. | `heap_min` |
| `psram` | PSRAM total e livre em bytes; zero quando indisponível. | `psram` |
| `flash` | Flash total, tamanho do sketch e espaço de sketch livre. | `flash` |

## Rede

| Comando | Função | Exemplo |
|---|---|---|
| `net_info` | IP, gateway, máscara e DNS de Wi-Fi e Ethernet, com estado das interfaces. | `net_info` |
| `net_monitor` | RTT atual/mínimo/médio/máximo, idade da medição, quedas, tempo offline e eventos. | `net_monitor` |
| `eth_status` | Estado do cabo, conexão Ethernet, IP, gateway e MAC. | `eth_status` |
| `wifi_status` | Estado Wi-Fi, código numérico de status/modo, SSID e IP STA/AP. | `wifi_status` |
| `channel` | Canal Wi-Fi. | `channel` |
| `mac` | MAC Wi-Fi. | `mac` |
| `rssi` | RSSI Wi-Fi em dBm. | `rssi` |
| `ip` | Endereços Wi-Fi e Ethernet. | `ip` |
| `ssid` | SSID conectado. | `ssid` |
| `wifi_list` | Somente SSIDs salvos, um por linha; lista vazia responde uma linha vazia. | `wifi_list` |
| `wifi_add:SSID\|SENHA` | Salva/atualiza uma rede; senha vazia cadastra rede aberta. Não reinicia. | `wifi_add:MinhaRede\|MinhaSenha123` |
| `reset_wifi` | Apaga todas as redes salvas e reinicia. | `reset_wifi` |

## OLED

| Comando | Função | Exemplo |
|---|---|---|
| `tela:0` | Relógio e data. | `tela:0` |
| `tela:1` | Wi-Fi: rede, IP, gateway, RSSI e canal. | `tela:1` |
| `tela:2` | Ethernet: IP, gateway, quedas e tempo offline. | `tela:2` |
| `tela:3` | Sistema e SD. | `tela:3` |
| `tela:4` | Ping aos gateways e gráfico. | `tela:4` |
| `tela:5` | Últimos quatro eventos de conexão. | `tela:5` |
| `tela:next` | Avança e desativa rotação automática. | `tela:next` |
| `tela:auto` | Retoma rotação a cada oito segundos. | `tela:auto` |

## Horário

| Comando | Função | Exemplo |
|---|---|---|
| `time` | Hora local. | `time` |
| `date` | Data local. | `date` |
| `ntp_status` | Validade do relógio, data/hora e servidores configurados; não comprova alcance atual do NTP. | `ntp_status` |
| `set_fuso:N` | Salva offset UTC inteiro de -12 a +14. | `set_fuso:-3` |
| `dst_on` | Salva horário de verão de uma hora. | `dst_on` |
| `dst_off` | Desativa e salva horário de verão. | `dst_off` |
| `dst:0 ou dst:1` | Define horário de verão explicitamente. | `dst:0` |

## LED

| Comando | Função | Exemplo |
|---|---|---|
| `led_on` | Acende e cancela sequência de pisca. | `led_on` |
| `led_off` | Apaga e cancela sequência de pisca. | `led_off` |
| `led_blink[:ms]` | Pisca durante a duração indicada, alternando a cada 200 ms, sem bloquear o loop. Padrão 3000 ms, máximo 10000 ms. | `led_blink:5000` |

## Cartão SD

| Comando | Função | Exemplo |
|---|---|---|
| `sd_status` | Capacidade total, usada e livre. | `sd_status` |
| `sd_list` | Lista arquivos na raiz; não recebe argumento de pasta nesta versão. | `sd_list` |
| `sd_read:arquivo` | Envia o arquivo inteiro em múltiplos pacotes UDP. | `sd_read:/dados.txt` |
| `sd_write:arquivo:texto` | Grava o texto substituindo o conteúdo. | `sd_write:/dados.txt:Primeira linha` |
| `sd_append:arquivo:texto` | Acrescenta texto e quebra de linha CRLF; cria o arquivo se necessário. | `sd_append:/dados.txt:Outra linha` |
| `sd_del:arquivo` | Remove arquivo. | `sd_del:/dados.txt` |
| `sd_exists:arquivo` | Verifica existência. | `sd_exists:/dados.txt` |
| `sd_size:arquivo` | Tamanho em bytes. | `sd_size:/dados.txt` |
| `sd_tail:arquivo[:linhas]` | Envia as últimas linhas; padrão 10. | `sd_tail:/dados.txt:5` |
| `sd_log[:linhas]` | Últimas linhas de /log.txt; padrão 10. | `sd_log:20` |
| `sd_clear_log` | Apaga /log.txt. | `sd_clear_log` |
| `sd_mkdir:pasta` | Cria pasta; a pasta pai precisa existir. | `sd_mkdir:/dados` |
| `sd_rmdir:pasta` | Remove pasta vazia; raiz não é permitida. | `sd_rmdir:/dados` |

## Aliases compatíveis

| Alias | Comando equivalente |
|---|---|
| `init` | `reason` |
| `flash_info` | `flash` |
| `restart` | `reboot` |
| `list` | `sd_list` |
| `read` | `sd_read` |
| `write` | `sd_write` |
| `delete` | `sd_del` |
| `exists` | `sd_exists` |
| `size` | `sd_size` |
| `tail` | `sd_tail` |
| `sd_info` | `sd_status` |
| `set_tz` | `set_fuso` |
| `set_dst` | `dst` |

## Cadastro circular de redes

```text
wifi_add:MinhaRede|MinhaSenha123
wifi_add:RedeAberta|
wifi_list
reboot
```

Novos SSIDs ocupam o próximo perfil circular, começando no perfil 1; após o quinto,
a próxima inserção substitui o perfil 1. Atualizar um SSID existente troca a senha
sem avançar o cursor. `wifi_add` verifica a gravação e tenta restaurar o perfil
anterior se falhar. `reboot` aplica a nova lista na busca de redes.

O cursor UDP é independente da última rede utilizada; o formulário HTTP edita
os cinco perfis diretamente e mantém o cursor. O SSID não pode conter `|`;
a senha pode conter `|`, pois só o primeiro separador é interpretado. SSID: de
1 a 32 bytes. Senha: de 8 a 63 bytes, 64 caracteres hexadecimais ou vazia para
rede aberta. Espaços da senha são preservados; CR/LF finais do comando são removidos.
Credenciais não aparecem no terminal, no log SD nem em `lastcmd`.

## Operações que alteram dados

`reboot`, `reset_wifi`, `wifi_add`, ajustes de fuso/DST e comandos de escrita,
exclusão e pastas SD alteram o dispositivo. O firmware executa diretamente;
a confirmação, quando houver, pertence ao aplicativo que envia os comandos.
`sd_write` substitui o arquivo, `sd_append` acrescenta uma linha e `sd_clear_log`
apaga o log. Operações NVS com vários campos não são atômicas contra falta de energia.

## Botão e portal

GPIO 2: toque curto avança tela; segure cinco segundos e solte para limpar redes
e reiniciar. `/info` mostra informações; `/wifi` edita os cinco perfis.

## Interpretação dos diagnósticos

Ping mede o gateway de cada interface, não o acesso à Internet. Um timeout pode
ser bloqueio de ICMP. Estatísticas do monitor ficam em RAM e reiniciam no boot.
`ntp_status` consulta a validade do relógio local, sem disparar nova consulta NTP.
`health` e `selftest` consultam estados já disponíveis; não testam escrita no SD
nem garantem a integridade física do hardware. Algumas operações SD continuam
bloqueantes e podem atrasar telas e monitor.

## Serial USB e terminal web (v1.8.0)

Todos os comandos deste documento também funcionam na serial, a **115200 baud**,
e no terminal da página `/info`. Na serial, envie uma linha por comando com final
LF, CR ou CRLF. Aguarde o fim da inicialização da placa. Limite: **254 bytes**.
Para a porta USB nativa, habilite **USB CDC On Boot** conforme a configuração da placa.

As respostas são enviadas ao canal de origem. No terminal web, respostas acima
de 8 KB são truncadas com um aviso; use `sd_tail` para consultar arquivos grandes.
O terminal pede confirmação para comandos que alteram arquivos/configurações.
Se a conexão cair, verifique o estado antes de repetir um comando de escrita.

