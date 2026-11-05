const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '..', 'PrometheusUi.h'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];
function browser({ ok = true, fail = false, pending = false, loadFail = false } = {}) {
  const nodes = new Map([...source.matchAll(/id='([^']+)'/g)].map(m => [m[1],{value:'',textContent:'',disabled:false}]));
  nodes.get('prom-save').disabled = true;
  const requests=[]; let finish;
  const saved={camera_stream_port:81,host:'prometheus.local',port:9120,active_port:9120,web_port:80,interval:30,mode:'pull',scrape_config:'scrape_configs:\n  - job_name: esp32_p4\n'};
  vm.runInNewContext(script, {
    document:{getElementById:id=>nodes.get(id)}, URLSearchParams, URL, AbortSignal,
    window:{location:{href:'http://192.168.1.50/prometheus'}},
    fetch:async(url,options)=>{
      requests.push({url,options});
      if(options.method!=='POST') return {ok:!loadFail,json:async()=>saved};
      if(fail)throw Error('offline');
      if(pending) await new Promise(resolve=>finish=resolve);
      return {ok,text:async()=>'Falha ao gravar',json:async()=>({...saved,host:options.body.get('host'),port:Number(options.body.get('port')),interval:Number(options.body.get('interval'))})};
    }
  });
  return {nodes,requests,get posts(){return requests.filter(r=>r.options.method==='POST')},finish(){finish()},
    flush:()=>new Promise(resolve=>setImmediate(resolve)),submit(){return nodes.get('prometheus-form').onsubmit({preventDefault(){}})}};
}
test('saved preferences and YAML load through the same API as M5NanoC6',async()=>{
  const b=browser();await b.flush();assert.equal(b.requests[0].url,'/api/prometheus/config');
  assert.equal(b.requests[0].options.cache,'no-store');assert.equal(b.nodes.get('prom-port').value,'9120');
  assert.equal(b.nodes.get('prom-interval').value,'30');assert.match(b.nodes.get('prom-yaml').value,/scrape_configs/);
  assert.equal(b.nodes.get('prom-save').disabled,false);
});
test('save sends host, exporter port and interval, keeping the page on port 80',async()=>{
  const b=browser();await b.flush();b.nodes.get('prom-port').value='9200';b.nodes.get('prom-interval').value='45';
  await b.submit();const r=b.posts[0];assert.equal(r.url,'/api/prometheus/config');
  assert.equal(r.options.headers['X-ESP32-Command'],'1');
  assert.deepEqual(Object.fromEntries(r.options.body),{host:'prometheus.local',port:'9200',interval:'45'});
  assert.equal(b.nodes.get('prom-link').href,'http://192.168.1.50:9200/metrics');
  assert.match(b.nodes.get('prom-status').textContent,/web continua na porta 80/);
});
test('port 80 shares the web and the UDP command port can also serve HTTP metrics',async()=>{
  for(const port of ['80','4210','65535']){
    const b=browser();await b.flush();b.nodes.get('prom-port').value=port;await b.submit();assert.equal(b.posts.length,1);
    const endpoint=new URL(b.nodes.get('prom-link').href);assert.equal(endpoint.hostname,'192.168.1.50');
    assert.equal(endpoint.port,port==='80'?'':port);
  }
});
test('OTA port, malformed ports and intervals outside 5–3600 are rejected',async()=>{
  for(const port of ['3232','0','65536','-1','9100abc','1.5','']){
    const b=browser();await b.flush();b.nodes.get('prom-port').value=port;await b.submit();assert.equal(b.posts.length,0,port);
  }
  for(const interval of ['0','4','3601','15x','1.5','']){
    const b=browser();await b.flush();b.nodes.get('prom-interval').value=interval;await b.submit();assert.equal(b.posts.length,0,interval);
  }
  for(const interval of ['5','3600']){
    const b=browser();await b.flush();b.nodes.get('prom-interval').value=interval;await b.submit();assert.equal(b.posts.length,1);
  }
});
test('host is required and URLs, markup and malformed labels cannot be saved',async()=>{
  for(const host of ['','http://server','server:9090','x/path',"x' onclick='x",'-bad.local','bad-.local','a..b','x'.repeat(64)+'.local']){
    const b=browser();await b.flush();b.nodes.get('prom-host').value=host;await b.submit();assert.equal(b.posts.length,0,host);
  }
});
test('failed preferences load prevents overwriting unknown saved settings',async()=>{
  const b=browser({loadFail:true});await b.flush();await b.submit();assert.equal(b.posts.length,0);
  assert.equal(b.nodes.get('prom-save').disabled,true);assert.match(b.nodes.get('prom-status').textContent,/ler as preferências/);
});
test('save failure remains visible and restores the button',async()=>{
  const b=browser({ok:false});await b.flush();await b.submit();
  assert.match(b.nodes.get('prom-status').textContent,/Falha/);assert.equal(b.nodes.get('prom-save').disabled,false);
});
test('network failure warns to check persisted state before repeating',async()=>{
  const b=browser({fail:true});await b.flush();await b.submit();
  assert.match(b.nodes.get('prom-status').textContent,/conferir a configuração/);
  assert.equal(b.nodes.get('prom-save').disabled,false);
});
test('repeated submits cannot overlap a pending preferences write',async()=>{
  const b=browser({pending:true});await b.flush();const first=b.submit();await b.submit();assert.equal(b.posts.length,1);
  assert.equal(b.nodes.get('prom-save').disabled,true);b.finish();await first;assert.equal(b.nodes.get('prom-save').disabled,false);
});
test('download link stays on web port while metrics link targets the device rather than the Prometheus server',async()=>{
  const b=browser();await b.flush();assert.match(source,/href='\/prometheus.yml' download/);
  assert.equal(b.nodes.get('prom-link').href,'http://192.168.1.50:9120/metrics');
});

test('metrics cannot claim the active camera streaming port', async () => {
 const b=browser();await b.flush();b.nodes.get('prom-port').value='81';await b.submit();assert.equal(b.posts.length,0);
});
