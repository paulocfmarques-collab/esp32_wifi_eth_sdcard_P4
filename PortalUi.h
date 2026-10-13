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
<div class='actions' id='quick-commands'><button type='button' class='secondary' data-command='status'>Estado</button><button type='button' class='secondary' data-command='health'>Diagnóstico</button><button type='button' class='secondary' data-command='protetor:wake'>Acordar tela</button><button type='button' class='secondary' data-command='led_on'>Ligar LED</button><button type='button' class='secondary' data-command='led_off'>Desligar LED</button><button type='button' class='secondary' data-command='tela:next'>Próxima tela</button><button type='button' class='secondary' data-command='sd_list'>Arquivos SD</button><button type='button' class='secondary' data-command='help'>Ajuda</button></div>
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
 if(/^(reboot|restart|reset_wifi|wifi_add|sd_write|write|sd_append|sd_del|delete|sd_clear_log|sd_mkdir|sd_rmdir|set_fuso|set_tz|dst|set_dst|dst_on|dst_off)$/.test(name)&&!confirm('Executar '+name+'? Essa ação altera o dispositivo.'))return;
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
static String portalStart(const char* title, bool wifi, bool logs = false) {
 String h; h.reserve(11000);
 h = F("<!doctype html><html lang='pt-BR'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
 h += "<title>" + String(title) + " · ESP32-P4</title>";
 h += FPSTR(PORTAL_STYLE);
 h += F("</head><body><div class='layout'><aside><a class='brand' href='/info'><span class='mark'>P4</span><span>Network Hub<small>ESP32 MONITOR</small></span></a><nav class='nav' aria-label='Navegação'>");
 h += String("<a href='/info' ") + (!wifi && !logs ? "class='active' aria-current='page'" : "") + ">Informações</a>";
 h += String("<a href='/wifi' ") + (wifi ? "class='active' aria-current='page'" : "") + ">Redes Wi-Fi</a>";
 h += F("<a href='/info#terminal'>Terminal</a>");
 h += String("<a href='/logs' ") + (logs ? "class='active' aria-current='page'" : "") + ">Log do dispositivo</a>";
 h += F("<a href='/firmware'>Firmware</a>");
 h += F("</nav><p class='aside-note'>Seu dispositivo, suas conexões.<br><br>Portal local · ESP32-P4</p></aside><main>");
 return h;
}
static String portalEnd() { return F("<footer class='footer'>ESP32-P4 · Wi-Fi / Ethernet / SD</footer></main></div></body></html>"); }
