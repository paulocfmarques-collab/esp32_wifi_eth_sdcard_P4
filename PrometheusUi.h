#pragma once
#include <Arduino.h>
static const char PROMETHEUS_FORM[] PROGMEM = R"PROM(
<div class='top'><div><div class='eyebrow'>Monitoramento</div><h1>Prometheus</h1><p class='muted'>O servidor consulta as métricas desta placa por HTTP. A interface web permanece na porta 80.</p></div></div>
<article class='card'><h2>Servidor e coleta</h2><form id='prometheus-form'>
<label class='field' for='prom-host'>Endereço do servidor Prometheus</label><input type='text' id='prom-host' maxlength='253' placeholder='192.168.1.10 ou prometheus.local' required autocomplete='off'>
<p class='hint'>Use IP ou nome de rede, sem http://, caminho ou porta. O endereço identifica o servidor, sem envio ativo ou restrição de acesso.</p>
<label class='field' for='prom-port'>Porta de métricas da placa</label><input type='text' inputmode='numeric' id='prom-port' value='80' required>
<label class='field' for='prom-interval'>Intervalo de coleta (segundos)</label><input type='text' inputmode='numeric' id='prom-interval' value='15' required>
<div class='actions'><button id='prom-save' disabled>Salvar nas preferências</button></div><p id='prom-status' role='status'>Lendo preferências…</p></form>
<p class='hint'>A porta é aplicada cerca de um segundo após salvar, sem reiniciar ou redirecionar a interface. Na porta 80, as métricas compartilham a web. A porta 3232 é reservada para OTA.</p><p id='prom-saved' class='hint'></p></article>
<article class='card section'><h2>Ativar no servidor</h2><p>Adicione este job ao prometheus.yml e recarregue o servidor. Salvar na placa não altera o servidor automaticamente.</p>
<textarea id='prom-yaml' readonly aria-label='Configuração de coleta Prometheus' style='width:100%;min-height:210px'></textarea>
<div class='actions'><a class='button secondary' href='/prometheus.yml' download>Baixar configuração</a><a class='button secondary' id='prom-link' href='/metrics' target='_blank' rel='noopener'>Ver métricas</a></div>
<p class='hint'>Se já existir scrape_configs, acrescente somente o item job_name. Use IP fixo ou reserva DHCP para a placa e um nome de job único.</p>
<p class='hint'>As métricas de sistema, Wi-Fi, CPU, memória, GPIO e SD continuam disponíveis. Métricas sem hardware/integração retornam NaN, com esp_metric_available. Use up do Prometheus para detectar a placa offline.</p></article>
<script>(()=>{
const byId=id=>document.getElementById(id), form=byId('prometheus-form'), status=byId('prom-status'), save=byId('prom-save');
let busy=false, loaded=false, streamPort=0;
const validNumber=(value,min,max)=>/^[0-9]{1,5}$/.test(value)&&Number(value)>=min&&Number(value)<=max;
function show(data){
streamPort=Number(data.camera_stream_port)||0;
byId('prom-host').value=data.host;byId('prom-port').value=String(data.port);byId('prom-interval').value=String(data.interval);byId('prom-yaml').value=data.scrape_config;
byId('prom-saved').textContent='Servidor: '+(data.host||'nenhum salvo')+' · Porta de métricas: '+data.port+' · Interface web: 80';
const endpoint=new URL('/metrics',window.location.href);endpoint.port=String(data.port);byId('prom-link').href=endpoint.href;
}
async function load(){
try{const response=await fetch('/api/prometheus/config',{cache:'no-store',signal:AbortSignal.timeout(10000)});if(!response.ok)throw Error('Não foi possível ler as preferências. Recarregue para tentar novamente.');show(await response.json());loaded=true;save.disabled=false;status.textContent='Preferências carregadas.'}
catch(error){status.textContent=error.message||'Falha ao ler preferências. Recarregue para tentar novamente.'}
}
form.onsubmit=async event=>{
event.preventDefault();if(busy||!loaded)return;
const port=byId('prom-port').value, interval=byId('prom-interval').value, host=byId('prom-host').value.trim();
if(!validNumber(port,1,65535)||Number(port)===3232||Number(port)===streamPort||!validNumber(interval,5,3600)){status.textContent='Use porta livre (exceto OTA e vídeo), com intervalo de 5–3600 segundos.';return}
if(!host||host.length>253||!host.split('.').every(part=>part.length<=63&&/^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$/i.test(part))){status.textContent='Use um hostname ou IPv4, sem http://, caminho ou porta.';return}
busy=true;save.disabled=true;status.textContent='Salvando e verificando…';
try{
const response=await fetch('/api/prometheus/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-ESP32-Command':'1'},body:new URLSearchParams({host,port,interval}),signal:AbortSignal.timeout(15000)});
if(!response.ok){status.textContent=await response.text();return}
show(await response.json());status.textContent='Preferências salvas. A web continua na porta 80; a porta de métricas será aplicada em cerca de um segundo. Aplique o YAML no servidor Prometheus.';
}catch(error){status.textContent='A resposta não chegou. Recarregue para conferir a configuração antes de salvar novamente.'}
finally{busy=false;save.disabled=false}
};
load();
})();</script>)PROM";
