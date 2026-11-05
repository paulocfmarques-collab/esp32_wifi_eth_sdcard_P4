const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '..', 'PortalUi.h'), 'utf8');
const html = source.match(/R"LOGS\(([\s\S]*?)\)LOGS"/)[1];
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
function browser() {
  const nodes = new Map([...html.matchAll(/id='([^']+)'/g)].map(m => [m[1], {
    value: '', textContent: '', checked: true, disabled: false, scrollTop: 0, scrollHeight: 500
  }]));
  nodes.get('log-lines').value = '100';
  const requests = [], intervals = [], listeners = {};
  let reply = { ok: true, data: { text: 'WiFi conectado\n[ERRO] SD\n', size: 123, lines: 2, limited: false } };
  const document = { hidden: false, getElementById: id => nodes.get(id), addEventListener: (event, cb) => listeners[event] = cb };
  vm.runInNewContext(script, {
    document, Date, AbortSignal, setInterval: cb => intervals.push(cb),
    fetch: async (url, options) => { requests.push({url, options}); return {ok: reply.ok, json: async () => reply.data}; }
  });
  return { nodes, requests, document, refresh: intervals[0], listeners,
    setReply(value) { reply = value; }, flush: () => new Promise(resolve => setImmediate(resolve)) };
}
test('initial load fetches the default tail without executing a command', async () => {
  const b=browser();await b.flush();assert.equal(b.requests[0].url,'/api/logs?lines=100');
  assert.equal(b.requests[0].options.cache,'no-store');assert.match(b.nodes.get('log-output').textContent,/WiFi conectado/);
  assert.equal(b.nodes.get('log-refresh').disabled,false);
});
test('logs containing HTML are rendered as text and search works on the loaded excerpt', async () => {
  const b=browser();await b.flush();b.setReply({ok:true,data:{text:'<script>alert(1)</script>\n[ERRO] WiFi\n',size:55,lines:2,limited:true}});
  await b.refresh();assert.match(b.nodes.get('log-output').textContent,/<script>/);
  b.nodes.get('log-search').value='erro';b.nodes.get('log-search').oninput();
  assert.equal(b.nodes.get('log-output').textContent,'[ERRO] WiFi');
  assert.match(b.nodes.get('log-message').textContent,/8 KB/);
});
test('automatic refresh stops while paused or hidden, but manual refresh still works', async () => {
  const b=browser();await b.flush();b.nodes.get('log-auto').checked=false;await b.refresh();assert.equal(b.requests.length,1);
  b.document.hidden=true;b.nodes.get('log-auto').checked=true;await b.refresh();assert.equal(b.requests.length,1);
  b.nodes.get('log-refresh').onclick();await b.flush();assert.equal(b.requests.length,2);
});
test('SD failure retains old text with a stale-data warning and enables retry', async () => {
  const b=browser();await b.flush();const old=b.nodes.get('log-output').textContent;
  b.setReply({ok:false,data:{error:'Cartão SD indisponível.'}});await b.refresh();
  assert.equal(b.nodes.get('log-output').textContent,old);assert.match(b.nodes.get('log-message').textContent,/desatualizado/);
  assert.equal(b.nodes.get('log-state').className,'badge warn');assert.equal(b.nodes.get('log-refresh').disabled,false);
});
test('changing line limit preserves search and optional scroll position', async () => {
  const b=browser();await b.flush();b.nodes.get('log-lines').value='250';b.nodes.get('log-follow').checked=false;
  b.nodes.get('log-output').scrollTop=10;b.nodes.get('log-search').value='erro';b.nodes.get('log-lines').onchange();await b.flush();
  assert.equal(b.requests[1].url,'/api/logs?lines=250');assert.equal(b.nodes.get('log-output').scrollTop,10);
  assert.equal(b.nodes.get('log-output').textContent,'[ERRO] SD');
});
test('empty file and malformed responses have distinct states', async () => {
  const b=browser();await b.flush();b.setReply({ok:true,data:{text:'',size:0,lines:0,limited:false}});await b.refresh();
  assert.equal(b.nodes.get('log-output').textContent,'Arquivo vazio.');
  b.setReply({ok:true,data:{}});await b.refresh();assert.match(b.nodes.get('log-message').textContent,/Resposta inválida/);
});
