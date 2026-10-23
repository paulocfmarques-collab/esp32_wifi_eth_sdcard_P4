#pragma once
#include <Arduino.h>

// Self-contained portal: no fonts, scripts or styles fetched from the Internet.
static const char PORTAL_STYLE[] PROGMEM = R"PORTAL(<style>
:root{color-scheme:dark;--bg:#0b1220;--panel:#131e30;--border:#26354d;--muted:#97a9c3;--ink:#edf4ff;--blue:#6ba8ff;--green:#55d7ad}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,Segoe UI,sans-serif}a{color:inherit;text-decoration:none}button,input{font:inherit}.layout{max-width:1240px;margin:auto;display:grid;grid-template-columns:220px 1fr;min-height:100vh}aside{padding:32px 22px;border-right:1px solid var(--border)}.brand{display:flex;gap:12px;align-items:center;font-weight:750;letter-spacing:.3px}.mark{background:#23436e;color:#a9cfff;border:1px solid #456b9c;border-radius:12px;padding:9px}.brand small{display:block;color:var(--muted);font-weight:400;font-size:11px;letter-spacing:2px}.nav{display:grid;gap:8px;margin:36px 0}.nav a{padding:12px 14px;border-radius:10px;color:var(--muted)}.nav a.active,.nav a:hover{background:#20334e;color:var(--ink)}.aside-note{font-size:12px;color:var(--muted);border-top:1px solid var(--border);padding-top:20px}main{padding:38px 36px;min-width:0}.eyebrow{text-transform:uppercase;color:var(--blue);font-size:11px;letter-spacing:2px;font-weight:700}h1{font-size:30px;line-height:1.2;margin:8px 0 10px;letter-spacing:-.8px}h2{font-size:16px;margin:0 0 16px}p{margin:0 0 16px}.muted,small{color:var(--muted)}.top{display:flex;justify-content:space-between;gap:20px;align-items:center;margin-bottom:28px}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}.metrics{grid-template-columns:repeat(4,minmax(0,1fr));margin-bottom:18px}.card{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:22px;min-width:0}.value{font-size:23px;font-weight:700;margin:10px 0 3px;overflow-wrap:anywhere}.label{color:var(--muted);font-size:12px}.badge{display:inline-flex;align-items:center;gap:6px;font-size:11px;line-height:1.4;padding:5px 9px;border-radius:20px;background:#253247;color:#b3c0d5}.badge.ok{background:#173a33;color:#75e9c3}.badge.warn{background:#413721;color:#f8d783}.badge::before{content:'';width:5px;height:5px;border-radius:50%;background:currentColor}.button,button{border:1px solid #3c78bc;background:#2d70bf;color:white;border-radius:10px;padding:10px 16px;cursor:pointer;display:inline-block;font-weight:600}.secondary{background:transparent;border-color:var(--border);color:var(--ink)}button:disabled{opacity:.6;cursor:wait}.row{display:flex;justify-content:space-between;gap:16px;padding:10px 0;border-bottom:1px solid var(--border);font-size:13px}.row:last-child{border:0}.row span{color:var(--muted)}.row strong{text-align:right;overflow-wrap:anywhere;min-width:0;font-weight:500}.section{margin-top:18px}.net-title{display:flex;justify-content:space-between;gap:12px;margin-bottom:14px}.net-title h2{margin:0}.slot{color:var(--blue);font-size:11px;letter-spacing:1px}.field{display:block;color:var(--muted);font-size:12px;margin:14px 0 6px}input[type=text],input[type=password]{width:100%;background:#0d1728;border:1px solid #34465f;color:var(--ink);padding:11px 12px;border-radius:9px;outline:none;min-width:0}input:focus{border-color:var(--blue);box-shadow:0 0 0 3px #6ba8ff18}.password{display:flex;gap:8px}.password button{font-size:11px;padding:8px 10px;background:#21334c;border-color:var(--border)}.check{display:flex;align-items:center;gap:8px;margin-top:12px;font-size:12px;color:var(--muted)}input[type=checkbox]{accent-color:var(--blue)}.hint{font-size:12px;color:var(--muted);margin:12px 0 0}.notice{background:#14283b;border:1px solid #2c5278;border-radius:12px;padding:16px 18px;margin-bottom:22px;font-size:13px;color:#c5ddf6}.actions{display:flex;align-items:center;gap:14px;flex-wrap:wrap;margin:22px 0}.actions p{margin:0;font-size:12px;color:var(--muted)}.full{grid-column:1/-1}.event{padding:10px 0;border-bottom:1px solid var(--border);font-family:ui-monospace,monospace;font-size:12px;overflow-wrap:anywhere}.event:last-child{border:0}.footer{color:var(--muted);font-size:11px;margin-top:26px}.success{border-color:#307058}.error{border-color:#865049}.message{max-width:600px;margin-top:30px}a:focus-visible,button:focus-visible{outline:2px solid var(--blue);outline-offset:3px}@media(max-width:1000px){.metrics{grid-template-columns:repeat(2,minmax(0,1fr))}.layout{grid-template-columns:180px 1fr}main{padding:28px 22px}}@media(max-width:700px){.layout{display:block}aside{padding:18px 20px;border-right:0;border-bottom:1px solid var(--border)}.nav{display:flex;margin:16px 0 0}.aside-note{display:none}main{padding:24px 18px}.top{align-items:flex-start;flex-direction:column}.grid{grid-template-columns:1fr}.metrics{grid-template-columns:repeat(2,minmax(0,1fr))}.card{padding:18px}h1{font-size:26px}.value{font-size:20px}}
</style>)PORTAL";
static const char PORTAL_CONSOLE[] PROGMEM = R"CONSOLE(
<div class='actions'><span class='badge ok' id='live-state' role='status'>Painel conectado</span><button class='secondary' id='pause-live' type='button'>Pausar atualização</button><small id='live-time'>Atualização a cada 5 segundos</small></div>
<article class='card section' id='terminal'><div class='net-title'><div><div class='eyebrow'>Controle do dispositivo</div><h2>Terminal de comandos</h2></div><span class='badge'>USB · UDP · Web</span></div>
<p class='muted'>Consulte o estado, controle o LED e explore os arquivos usando os mesmos comandos da serial.</p>
<div class='actions' id='quick-commands'><button type='button' class='secondary' data-command='status'>Estado</button><button type='button' class='secondary' data-command='health'>Diagnóstico</button><button type='button' class='secondary' data-command='tela:auto'>Telas automáticas</button><button type='button' class='secondary' data-command='protetor:on'>Carinhas de robô</button><button type='button' class='secondary' data-command='audio_test'>Testar som</button><button type='button' class='secondary' data-command='camera_oled'>Câmera no OLED</button><button type='button' class='secondary' data-command='weather_status'>Clima externo</button><button type='button' class='secondary' data-command='led_on'>Ligar LED</button><button type='button' class='secondary' data-command='led_off'>Desligar LED</button><button type='button' class='secondary' data-command='tela:next'>Próxima tela</button><button type='button' class='secondary' data-command='sd_list'>Arquivos SD</button><button type='button' class='secondary' data-command='help'>Ajuda</button></div>
<pre id='console-output' role='log' aria-label='Respostas dos comandos' tabindex='0'>Pronto. Digite help para listar os comandos.
</pre><form id='command-form'><label class='field' for='command-input'>Comando</label><div class='command-line'><input type='text' id='command-input' autocomplete='off' spellcheck='false' placeholder='Ex.: status ou sd_tail:/log.txt:10' maxlength='254' required><button id='command-send' type='submit'>Enviar</button></div></form>
<div class='actions'><button type='button' class='secondary' id='clear-console'>Limpar terminal</button><small>Serial USB: 115200 baud · Termine cada comando com Enter (LF ou CR).</small></div><p class='hint'>O terminal controla este dispositivo pela rede local. Comandos que alteram arquivos ou configurações pedem confirmação. Respostas web limitadas a 8 KB.</p></article>
<style>
html{scroll-behavior:smooth}body{background:radial-gradient(ellipse at top right,#18314a 0,transparent 48%),var(--bg)}.card{box-shadow:0 8px 30px #0002}.card h2{letter-spacing:-.2px}.metrics .card{border-top:2px solid #5186bb}.metrics .value{letter-spacing:-.6px}.command-line{display:flex;gap:10px}#console-output{background:#080f1b;border:1px solid var(--border);border-radius:12px;padding:18px;min-height:190px;max-height:420px;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;color:#b4f3d5;font:13px/1.65 ui-monospace,Consolas,monospace}.actions button{font-size:12px}#command-input{font-family:ui-monospace,Consolas,monospace}button:hover{filter:brightness(1.15)}@media(max-width:700px){.command-line{flex-direction:column}.nav{flex-wrap:wrap}}@media(prefers-reduced-motion:reduce){html{scroll-behavior:auto}}
</style><script>
(()=>{
const byId=id=>document.getElementById(id), output=byId('console-output'), input=byId('command-input');
let paused=false,busy=false,refreshing=false;const history=[];let position=0;
function write(text){output.textContent=(output.textContent+text).slice(-24000);output.scrollTop=output.scrollHeight}
async function refresh(){
 if(paused||document.hidden||busy||refreshing)return;
 refreshing=true;
 try{const response=await fetch('/api/dashboard',{cache:'no-store',signal:AbortSignal.timeout(10000)});if(!response.ok)throw Error();
 const page=new DOMParser().parseFromString(await response.text(),'text/html'), next=page.getElementById('dashboard');if(!next)throw Error();
 byId('dashboard').replaceWith(next);byId('live-state').textContent='Painel conectado';byId('live-state').className='badge ok';byId('live-time').textContent='Atualizado às '+new Date().toLocaleTimeString('pt-BR');
 }catch(error){byId('live-state').textContent='Sem resposta do dispositivo';byId('live-state').className='badge warn';byId('live-time').textContent='Tentando novamente a cada 5 segundos';}finally{refreshing=false}
}
byId('pause-live').onclick=()=>{paused=!paused;byId('pause-live').textContent=paused?'Retomar atualização':'Pausar atualização';if(!paused)refresh()};
async function send(command){
 if(busy)return;
 if(new TextEncoder().encode(command).length>254||/[\r\n]/.test(command)){write('\nErro: use até 254 bytes e uma única linha.\n');return}
 const name=command.split(':')[0].trim().toLowerCase();
 if(/^(reboot|restart|reset_wifi|reset_counters|wifi_add|sd_write|write|sd_append|sd_del|delete|sd_clear_log|sd_mkdir|sd_rmdir|set_fuso|set_tz|dst|set_dst|dst_on|dst_off|crash_info)$/.test(name)&&!confirm(name==='crash_info'?'Arquivar o crash no SD e apagar o registro interno somente após verificar a cópia?':'Executar '+name+'? Essa ação altera o dispositivo.'))return;
 const sensitive=name==='wifi_add';if(!sensitive){history.push(command);if(history.length>50)history.shift()}position=history.length;
 write('\n› '+(sensitive?'wifi_add:[credenciais ocultas]':command)+'\n');busy=true;input.value='';
 const buttons=[...byId('terminal').querySelectorAll('button')];buttons.forEach(b=>b.disabled=true);
 try{const response=await fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-ESP32-Command':'1'},body:new URLSearchParams({cmd:command}),signal:AbortSignal.timeout(30000)});const text=await response.text();write((response.ok?'':'Erro HTTP '+response.status+'\n')+text);
 }catch(error){write('A resposta não chegou. Verifique a conexão ou o reinício da placa. O comando pode ter sido executado; confira o estado antes de repetir.\n')}
 finally{busy=false;buttons.forEach(b=>b.disabled=false);input.focus();refresh()}
}
byId('command-form').onsubmit=event=>{event.preventDefault();if(input.value.trim())send(input.value)};
byId('quick-commands').onclick=event=>{const button=event.target.closest('[data-command]');if(button)send(button.dataset.command)};
byId('clear-console').onclick=()=>{output.textContent=''};
input.onkeydown=event=>{if(event.key==='ArrowUp'||event.key==='ArrowDown'){event.preventDefault();position=Math.max(0,Math.min(history.length,position+(event.key==='ArrowUp'?-1:1)));input.value=history[position]||''}};
setInterval(refresh,5000);document.addEventListener('visibilitychange',()=>{if(!document.hidden)refresh()});
})();
</script>)CONSOLE";
static const char PORTAL_FIRMWARE[] PROGMEM = R"OTA(
<form id='firmware-form'><label class='field' for='firmware-file'>Firmware do aplicativo</label><input id='firmware-file' type='file' accept='.bin' required><div class='actions'><button id='firmware-send' type='submit'>Enviar firmware</button><a class='button secondary' href='/info'>Voltar ao painel</a></div><progress id='firmware-progress' max='100' value='0' style='width:100%;accent-color:var(--green)'></progress><p id='firmware-status' role='status' class='hint'>Selecione o arquivo .bin exportado para esta placa.</p></form>
<script>(()=>{
const file=document.getElementById('firmware-file'), form=document.getElementById('firmware-form'), button=document.getElementById('firmware-send'), progress=document.getElementById('firmware-progress'), status=document.getElementById('firmware-status');
let sending=false;
form.onsubmit=event=>{event.preventDefault();if(sending)return;const firmware=file.files[0];
if(!firmware||!firmware.name.endsWith('.bin')||!firmware.size||firmware.size>window.firmwareLimit){status.textContent='Selecione um .bin válido que caiba na partição OTA.';return}
if(!confirm('Atualizar o firmware e reiniciar a placa? Mantenha a alimentação até concluir.'))return;
sending=true;button.disabled=true;file.disabled=true;progress.value=0;status.textContent='Enviando firmware…';
const request=new XMLHttpRequest();request.open('POST','/api/firmware?size='+firmware.size);request.setRequestHeader('X-ESP32-Firmware','1');request.timeout=120000;
request.upload.onprogress=event=>{if(event.lengthComputable){progress.value=Math.round(event.loaded/event.total*100);status.textContent='Enviando: '+progress.value+'%'}};
request.upload.onload=()=>{status.textContent='Transferência enviada. Verificando firmware…'};
function reset(){sending=false;button.disabled=false;file.disabled=false}
request.onload=()=>{status.textContent=request.responseText;if(request.status===200){progress.value=100;sending=false;button.textContent='Firmware enviado';file.value=''}else reset()};
request.onerror=request.ontimeout=()=>{status.textContent='A resposta não chegou. Confira a serial e a versão da placa antes de tentar novamente.';reset()};
request.onabort=()=>{status.textContent='Transferência interrompida. Confira a versão da placa.';reset()};
const data=new FormData();data.append('firmware',firmware);request.send(data);
};
window.addEventListener('beforeunload',event=>{if(sending){event.preventDefault();event.returnValue=''}});
})();</script>)OTA";
static const char PORTAL_LOGS[] PROGMEM = R"LOGS(
<div class='top'><div><div class='eyebrow'>Diagnóstico</div><h1>Log do dispositivo</h1><p class='muted'>Eventos e comandos registrados no cartão SD.</p></div><span class='badge' id='log-state' role='status'>Carregando…</span></div>
<article class='card'><div class='net-title'><h2>/log.txt</h2><small id='log-meta'>Aguardando leitura</small></div>
<div class='actions'><label for='log-lines'>Últimas linhas</label><select id='log-lines'><option value='25'>25</option><option value='100' selected>100</option><option value='250'>250</option></select><button type='button' id='log-refresh'>Atualizar agora</button><label class='check'><input type='checkbox' id='log-auto' checked>Atualizar a cada 5 s</label><label class='check'><input type='checkbox' id='log-follow' checked>Acompanhar o final</label></div>
<label class='field' for='log-search'>Buscar no trecho exibido</label><input type='text' id='log-search' placeholder='Ex.: WiFi, ERRO ou um comando' autocomplete='off'>
<p class='hint' id='log-message' role='status'>Consultando o dispositivo…</p><pre id='log-output' tabindex='0' aria-label='Conteúdo do log'></pre><p class='hint'>São lidos somente os últimos 8 KB do arquivo. A busca filtra esse trecho. Consultas não gravam eventos no log nem acordam o OLED.</p></article>
<style>#log-output{background:#080f1b;color:#c9dfef;border:1px solid var(--border);border-radius:12px;padding:18px;min-height:220px;max-height:65vh;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;font:13px/1.65 ui-monospace,Consolas,monospace}#log-lines{font:inherit;background:var(--panel);color:var(--ink);border:1px solid var(--border);border-radius:8px;padding:8px}.actions .check{margin:0}</style>
<script>(()=>{
const el=id=>document.getElementById(id), output=el('log-output'), state=el('log-state'), message=el('log-message');
let busy=false, text='', hasData=false;
function render(){const query=el('log-search').value.toLocaleLowerCase('pt-BR');let result=text;
 if(query)result=text.split('\n').filter(line=>line.toLocaleLowerCase('pt-BR').includes(query)).join('\n');
 output.textContent=result||(query?'Nenhuma linha corresponde à busca.':hasData?'Arquivo vazio.':'');
 if(el('log-follow').checked)output.scrollTop=output.scrollHeight;
}
async function load(manual=false){
 if(busy||(!manual&&(!el('log-auto').checked||document.hidden)))return;
 busy=true;el('log-refresh').disabled=true;
 try{const response=await fetch('/api/logs?lines='+el('log-lines').value,{cache:'no-store',signal:AbortSignal.timeout(10000)});
 const data=await response.json();if(!response.ok)throw Error(data.error||'Falha ao ler o log.');
 if(typeof data.text!=='string'||typeof data.size!=='number'||typeof data.lines!=='number')throw Error('Resposta inválida do dispositivo.');
 text=data.text;hasData=true;render();state.textContent='Log atualizado';state.className='badge ok';
 el('log-meta').textContent=data.size.toLocaleString('pt-BR')+' bytes · '+data.lines+' linhas no trecho';
 message.textContent='Atualizado às '+new Date().toLocaleTimeString('pt-BR')+(data.limited?' · Trecho limitado às linhas selecionadas ou aos últimos 8 KB.':'');
 }catch(error){state.textContent='Leitura indisponível';state.className='badge warn';message.textContent=(error.name==='TimeoutError'?'O dispositivo não respondeu a tempo.':error.message)+(hasData?' O trecho anterior permanece visível e pode estar desatualizado.':'');}
 finally{busy=false;el('log-refresh').disabled=false}
}
el('log-refresh').onclick=()=>load(true);el('log-lines').onchange=()=>load(true);el('log-search').oninput=render;
el('log-auto').onchange=()=>{if(el('log-auto').checked)load()};el('log-follow').onchange=render;
document.addEventListener('visibilitychange',()=>{if(!document.hidden)load()});setInterval(load,5000);load(true);
})();</script>)LOGS";

static const char MQTT_FORM[] PROGMEM = R"MQTT(
<form id='mqtt-form'><fieldset id='mqtt-fields' disabled>
<label><input id='mqtt-enabled' type='checkbox'> Ativar MQTT</label>
<label>Servidor MQTT<input id='mqtt-host' maxlength='253' placeholder='192.168.0.10' autocomplete='off'></label>
<label>Porta<input id='mqtt-port' type='number' min='1' max='65535' value='1883' required></label>
<label>Usuário<input id='mqtt-user' maxlength='128' autocomplete='username'></label>
<label>Senha<input id='mqtt-password' type='password' maxlength='128' autocomplete='new-password' placeholder='Deixe vazio para manter'></label>
<label><input id='mqtt-clear' type='checkbox'> Apagar senha salva</label>
<button id='mqtt-save' type='submit'>Salvar MQTT</button></fieldset></form>
<p id='mqtt-result' role='status' aria-live='polite'>Carregando configuração…</p>
<p>A integração MQTT deve usar o mesmo broker no Home Assistant. A senha salva não é devolvida à página.</p>
<script>(()=>{'use strict';
const el=id=>document.getElementById('mqtt-'+id); let loaded=false,busy=false;
const show=c=>{el('enabled').checked=c.enabled;el('host').value=c.host;el('port').value=c.port;el('user').value=c.user;el('password').value='';el('clear').checked=false;
el('result').textContent=(c.connected?'Conectado. ':'Desconectado. ')+(c.password_set?'Senha salva. ':'Sem senha salva. ')+'Tópico: '+c.base_topic;};
async function load(){try{const r=await fetch('/api/mqtt/config',{cache:'no-store',signal:AbortSignal.timeout(8000)});if(!r.ok)throw Error('Falha ao carregar');show(await r.json());loaded=true;el('fields').disabled=false;}catch(e){el('result').textContent='Não foi possível carregar. Recarregue a página antes de salvar.';}}
el('form').onsubmit=async e=>{e.preventDefault();if(!loaded||busy)return;
const host=el('host').value.trim(),port=el('port').value;
if((host&&!/^[a-zA-Z0-9.-]{1,253}$/.test(host))||(el('enabled').checked&&!host)||!/^\d{1,5}$/.test(port)||Number(port)<1||Number(port)>65535){el('result').textContent='Confira servidor e porta.';return;}
busy=true;el('save').disabled=true;
const body=new URLSearchParams({enabled:el('enabled').checked?'1':'0',host,port,user:el('user').value,password:el('password').value,clear_password:el('clear').checked?'1':'0'});
try{const r=await fetch('/api/mqtt/config',{method:'POST',headers:{'X-ESP32-Command':'1'},body,signal:AbortSignal.timeout(10000)});if(!r.ok)throw Error(await r.text());show(await r.json());el('result').textContent+=' Configuração salva. Aguarde a reconexão e recarregue para conferir o estado.';}
catch(e){el('result').textContent='Não foi possível confirmar: '+e.message+'. Recarregue para conferir o que ficou salvo.';}
finally{busy=false;el('save').disabled=false;}};load();})();</script>
)MQTT";

static const char EDGE_FORM[] PROGMEM = R"EDGE(
<form id='edge-form'><fieldset id='edge-fields' disabled>
<label><input id='edge-enabled' type='checkbox'> Ativar regras locais</label>
<label>Alerta de CPU a partir de (°C)<input id='edge-cpu_limit' type='number' min='20' max='100' required></label>
<label>Alerta de memória livre abaixo de (bytes)<input id='edge-heap_limit' type='number' min='8192' max='1048576' required></label>
<label>Falhas de envio UDP em 60 segundos<input id='edge-udp_limit' type='number' min='1' max='10000' required></label>
<label>Confirmar condição por (segundos)<input id='edge-confirm_seconds' type='number' min='1' max='600' required></label>
<label>Intervalo mínimo entre alertas da mesma regra (segundos)<input id='edge-cooldown_seconds' type='number' min='30' max='86400' required></label>
<label><input id='edge-sound' type='checkbox'> Emitir som nos alertas</label>
<button id='edge-save' type='submit'>Salvar regras</button></fieldset></form>
<p id='edge-result' role='status' aria-live='polite'>Carregando regras…</p>
<p>As regras começam desativadas. Rede e UDP aguardam 60 segundos para formar a primeira janela. A recuperação também exige confirmação: CPU 3 °C abaixo do limite e memória 8192 bytes acima. Salvar reinicia as avaliações e os alertas ativos.</p>
<h2>Estado das regras</h2><pre id='edge-status'>Carregando estado…</pre>
<h2>Últimos eventos</h2><pre id='edge-history'>Carregando histórico…</pre>
<script>(()=>{'use strict';
const el=id=>document.getElementById('edge-'+id),limits={cpu_limit:[20,100],heap_limit:[8192,1048576],udp_limit:[1,10000],confirm_seconds:[1,600],cooldown_seconds:[30,86400]};
const names={cpu_hot:'CPU quente',heap_low:'Memória baixa',network_down:'Rede indisponível',udp_errors:'Falhas UDP'};
let loaded=false,busy=false;
function show(c){el('enabled').checked=c.enabled;el('sound').checked=c.sound;for(const key of Object.keys(limits))el(key).value=c[key];}
async function status(){if(document.hidden)return;try{const r=await fetch('/api/edge/status',{cache:'no-store',signal:AbortSignal.timeout(8000)});if(!r.ok)throw Error();const s=await r.json();
el('status').textContent=(s.enabled?'Regras ativadas':'Regras desativadas')+(s.paused?' · Pausadas durante atualização':'')+'\n'+s.rules.map(r=>(names[r.rule]||r.rule)+': '+(!r.valid?'aguardando dados':r.active?'ALERTA':'normal')+(r.pending?' · confirmando mudança':'')).join('\n');
el('history').textContent=s.history.length?s.history.map(e=>'#'+e.sequence+' · '+e.uptime+' s ligado · '+(names[e.rule]||e.rule)+' · '+(e.active?'ALERTA':'RECUPERADO')).join('\n'):'Nenhum evento neste boot.';
}catch(e){el('status').textContent='Não foi possível atualizar. Os dados anteriores podem estar desatualizados.';}}
async function load(){try{const r=await fetch('/api/edge/config',{cache:'no-store',signal:AbortSignal.timeout(8000)});if(!r.ok)throw Error();show(await r.json());loaded=true;el('fields').disabled=false;el('result').textContent='Configuração carregada.';}catch(e){el('result').textContent='Falha ao carregar. Recarregue antes de salvar.';}}
el('form').onsubmit=async e=>{e.preventDefault();if(!loaded||busy)return;
const body=new URLSearchParams({enabled:el('enabled').checked?'1':'0',sound:el('sound').checked?'1':'0'});
for(const [key,[min,max]]of Object.entries(limits)){const value=String(el(key).value);if(!/^\d{1,8}$/.test(value)||Number(value)<min||Number(value)>max){el('result').textContent='Confira limites e tempos.';return;}body.set(key,value);}
busy=true;el('save').disabled=true;
try{const r=await fetch('/api/edge/config',{method:'POST',headers:{'X-ESP32-Command':'1'},body,signal:AbortSignal.timeout(10000)});if(!r.ok)throw Error(await r.text());show(await r.json());el('result').textContent='Regras salvas. Avaliação reiniciada.';await status();}
catch(e){el('result').textContent='Não foi possível confirmar: '+e.message+'. Recarregue para conferir o que ficou salvo.';}
finally{busy=false;el('save').disabled=false;}};load();status();setInterval(status,5000);})();</script>
)EDGE";

static const char CAMERA_VIEW[] PROGMEM = R"CAMERA(
<div class='actions'><button id='camera-live' type='button'>Iniciar vídeo</button><button id='camera-refresh' type='button' class='secondary'>Foto JPEG</button><button id='camera-pause' type='button' class='secondary'>Pausar visualização</button><a href='/camera.jpg' target='_blank' rel='noopener'>Abrir JPEG</a><a id='camera-stream-link' target='_blank' rel='noopener'>Abrir streaming</a></div>
<p id='camera-result' role='status' aria-live='polite'>Aguardando imagem…</p>
<p>Endereço do vídeo para o Home Assistant: <code id='camera-url'>aguardando…</code></p>
<img id='camera-image' alt='Imagem da câmera OV5647' style='display:block;max-width:100%;height:auto;border-radius:12px'>
<script>(()=>{'use strict';
const el=id=>document.getElementById('camera-'+id);let paused=false,busy=false,previous=null,mode='live',streaming=false,retryAt=0;
function stop(){el('image').removeAttribute('src');streaming=false;if(previous){URL.revokeObjectURL(previous);previous=null;}}
async function video(){if(busy||paused||document.hidden)return;busy=true;
try{const r=await fetch('/api/camera/status',{cache:'no-store',signal:AbortSignal.timeout(8000)});if(!r.ok)throw Error('Não foi possível consultar a câmera');const s=await r.json();
if(!Number.isInteger(s.stream_port)||s.stream_port<1||s.stream_port>65535)throw Error('Servidor de vídeo indisponível; consulte camera_status');
const url=new URL('/stream',window.location.href);url.port=String(s.stream_port);el('url').textContent=url.href;el('stream-link').href=url.href;
if(!s.available)throw Error('Câmera ainda sem imagem recente ou pausada');
if(paused||document.hidden||mode!=='live')return;stop();url.searchParams.set('t',String(Date.now()));streaming=true;el('image').src=url.href;el('result').textContent='Conectando ao vídeo MJPEG…';}
catch(e){streaming=false;retryAt=Date.now()+3000;el('result').textContent=e.message+'. Nova tentativa automática em alguns segundos.';}
finally{busy=false;}}
el('image').onload=()=>{if(mode==='live'&&streaming)el('result').textContent='Vídeo MJPEG ativo · até 2 imagens por segundo.';};
el('image').onerror=()=>{if(mode==='live'&&!paused){stop();retryAt=Date.now()+3000;el('result').textContent='Vídeo interrompido ou limite de espectadores atingido. Reconectando…';}};
async function refresh(manual=false){if(busy||document.hidden||(!manual&&paused))return;busy=true;el('refresh').disabled=true;
if(manual){mode='snapshot';stop();}
try{const r=await fetch('/camera.jpg',{cache:'no-store',signal:AbortSignal.timeout(8000)});if(!r.ok)throw Error(await r.text());
const blob=await r.blob();if(blob.type!=='image/jpeg'||!blob.size)throw Error('Resposta sem imagem JPEG');
if(document.hidden)return;
const url=URL.createObjectURL(blob);el('image').src=url;if(previous)URL.revokeObjectURL(previous);previous=url;
el('result').textContent='Imagem atualizada. '+(paused?'Visualização pausada.':'Atualização a cada segundo.');}
catch(e){el('result').textContent='Sem imagem recente: '+e.message+'. A imagem anterior, se houver, está desatualizada. Consulte camera_status.';}
finally{busy=false;el('refresh').disabled=false;}}
el('live').onclick=()=>{mode='live';paused=false;retryAt=0;el('pause').textContent='Pausar visualização';stop();video();};
el('refresh').onclick=()=>refresh(true);el('pause').onclick=()=>{paused=!paused;el('pause').textContent=paused?'Retomar visualização':'Pausar visualização';if(paused){if(mode==='live')stop();el('result').textContent='Visualização pausada. A captura na placa continua.';}else if(mode==='live')video();else refresh();};
document.addEventListener('visibilitychange',()=>{if(document.hidden){if(mode==='live')stop();}else if(mode==='live')video();else refresh();});
window.addEventListener('pagehide',stop);
video();setInterval(()=>{if(mode==='live'){if(!streaming&&Date.now()>=retryAt)video();}else refresh();},1000);})();</script>
)CAMERA";

static const char USB_VIEW[] PROGMEM = R"USBVIEW(
<header class='page-header'><h1>USB Host e registros</h1></header><section class='card'>
<p>Teclado, mouse e um pendrive FAT16/FAT32. Jumper DEVICE: portas 2–4 via hub; HOST: somente porta 1 direta. USB-C de programação permanece separado. O pendrive não é formatado e não recebe os logs: eles continuam no SD.</p>
<p>Relatórios de teclado/mouse são contados; texto e teclas não são gravados nem executados como comandos.</p>
<button id='usb-refresh' type='button'>Atualizar estado</button><button id='usb-rescan' type='button' class='secondary'>Reler raiz do pendrive</button>
<p id='usb-message' role='status' aria-live='polite'></p><pre id='usb-status'>Carregando…</pre>
<h2>Raiz do pendrive (até 32 entradas)</h2><pre id='usb-files'>Carregando…</pre>
<h2>Fila de log do SD</h2><pre id='usb-logs'>Carregando…</pre><a href='/logs'>Abrir log do SD</a>
</section><script>(()=>{'use strict';
const el=id=>document.getElementById('usb-'+id);let busy=false;
const states=['Não iniciado','Host ativo','Falha de tarefa/memória','Pacote sem suporte host/HID/MSC','Conflito com USB Device no boot','Falha ao iniciar o host'];
async function refresh(){if(busy||document.hidden)return;busy=true;el('refresh').disabled=true;
try{const requests=await Promise.all(['/api/usb/status','/api/usb/files','/api/log/status'].map(url=>fetch(url,{cache:'no-store',signal:AbortSignal.timeout(8000)})));if(requests.some(r=>!r.ok))throw Error('Falha ao consultar USB/log');
const [s,files,l]=await Promise.all([requests[0].json(),requests[1].text(),requests[2].json()]);
el('status').textContent=(states[s.state]||'Estado desconhecido')+'\nTeclado: '+(s.keyboard?'conectado':'ausente')+' · relatórios: '+s.keyboard_reports+'\nMouse: '+(s.mouse?'conectado':'ausente')+' · relatórios: '+s.mouse_reports+'\nPendrive: '+(s.drive_connected?'conectado':'ausente')+' · FAT: '+(s.fat_mounted?'montado':'não montado')+'\nCapacidade: '+s.drive_bytes+' bytes · erros USB: '+s.errors;
el('files').textContent=files;el('logs').textContent='Fila: '+l.queued+' · gravados: '+l.written+' · descartados: '+l.dropped+'\nRotação: 1 MB por arquivo, quatro históricos.';el('message').textContent='Estado atualizado.';
}catch(e){el('message').textContent='Não foi possível atualizar. Os dados anteriores podem estar desatualizados.';}finally{busy=false;el('refresh').disabled=false;}}
el('refresh').onclick=refresh;el('rescan').onclick=async()=>{el('rescan').disabled=true;try{const r=await fetch('/api/command',{method:'POST',headers:{'X-ESP32-Command':'1'},body:new URLSearchParams({cmd:'usb_rescan'}),signal:AbortSignal.timeout(8000)});if(!r.ok)throw Error();el('message').textContent=await r.text();}catch(e){el('message').textContent='Não foi possível confirmar a releitura; consulte usb_status.';}finally{el('rescan').disabled=false;}};
refresh();setInterval(refresh,5000);})();</script>
)USBVIEW";

static String portalStart(const char* title, bool wifi, bool logs = false, bool prometheus = false, bool mqtt = false, bool edge = false, bool camera = false, bool usb = false)
{
    String h;
    h.reserve(11000);
    h = F("<!doctype html><html lang='pt-BR'><head><meta charset='utf-8'><meta name='viewport' "
          "content='width=device-width,initial-scale=1'>");
    h += "<title>" + String(title) + " · ESP32-P4</title>";
    h += FPSTR(PORTAL_STYLE);
    h += F("</head><body><div class='layout'><aside><a class='brand' href='/info'><span "
           "class='mark'>P4</span><span>Network Hub<small>ESP32 MONITOR</small></span></a><nav "
           "class='nav' aria-label='Navegação'>");
    h += String("<a href='/info' ") + (!wifi && !logs && !prometheus && !mqtt && !edge && !camera && !usb ? "class='active' aria-current='page'" : "") +
         ">Informações</a>";
    h += String("<a href='/wifi' ") + (wifi ? "class='active' aria-current='page'" : "") +
         ">Redes Wi-Fi</a>";
    h += F("<a href='/info#terminal'>Terminal</a>");
    h += String("<a href='/logs' ") + (logs ? "class='active' aria-current='page'" : "") +
         ">Log do dispositivo</a>";
    h += F("<a href='/firmware'>Firmware</a>");
    h += String("<a href='/prometheus' ") + (prometheus ? "class='active' aria-current='page'" : "") + ">Prometheus</a>";
    h += String("<a href='/mqtt' ") + (mqtt ? "class='active' aria-current='page'" : "") + ">MQTT / Home Assistant</a>";
    h += String("<a href='/edge' ") + (edge ? "class='active' aria-current='page'" : "") + ">Processamento local</a>";
    h += String("<a href='/camera' ") + (camera ? "class='active' aria-current='page'" : "") + ">Câmera</a>";
    h += String("<a href='/usb' ") + (usb ? "class='active' aria-current='page'" : "") + ">USB / Registros</a>";
    h += F("</nav><p class='aside-note'>Seu dispositivo, suas conexões.<br><br>Portal local · "
           "ESP32-P4</p></aside><main>");
    return h;
}

static String portalEnd()
{
    return F("<footer class='footer'>ESP32-P4 · Wi-Fi / Ethernet / "
             "SD</footer></main></div></body></html>");
}
