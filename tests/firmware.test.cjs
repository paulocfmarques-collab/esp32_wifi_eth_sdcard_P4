const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '..', 'PortalUi.h'), 'utf8');
const html = source.match(/R"OTA\(([\s\S]*?)\)OTA"/)[1];
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
function page({ approve = true, firmware = { name: 'app.bin', size: 512 } } = {}) {
  const nodes = new Map([...html.matchAll(/id='([^']+)'/g)].map(match => [match[1], {
    value: '', textContent: '', disabled: false, files: [firmware]
  }]));
  const requests = [], listeners = {};
  class Request {
    constructor() { this.upload = {}; this.headers = {}; requests.push(this); }
    open(method, url) { this.method = method; this.url = url; }
    setRequestHeader(name, value) { this.headers[name] = value; }
    send(data) { this.data = data; }
  }
  const window = { firmwareLimit: 1024, addEventListener: (name, fn) => listeners[name] = fn };
  vm.runInNewContext(script, {
    document: { getElementById: id => nodes.get(id) }, window,
    confirm: () => approve, XMLHttpRequest: Request,
    FormData: class { append(name, value) { this.name = name; this.value = value; } }
  });
  return { nodes, requests, listeners, submit() { nodes.get('firmware-form').onsubmit({ preventDefault() {} }); } };
}
test('invalid file, zero length and oversized application never upload', () => {
  for (const firmware of [{name:'app.txt',size:512},{name:'app.bin',size:0},{name:'app.bin',size:1025}]) {
    const p=page({firmware});p.submit();assert.equal(p.requests.length,0);
  }
});
test('firmware update requires explicit confirmation', () => {
  const p=page({approve:false});p.submit();assert.equal(p.requests.length,0);
});
test('upload sends exact size, one named file, and cross-origin protection header', () => {
  const p=page();p.submit();p.submit();assert.equal(p.requests.length,1);
  const r=p.requests[0];assert.equal(r.url,'/api/firmware?size=512');
  assert.equal(r.headers['X-ESP32-Firmware'],'1');assert.equal(r.data.name,'firmware');
  assert.equal(p.nodes.get('firmware-send').disabled,true);
});
test('upload progress does not claim firmware was activated before verification', () => {
  const p=page();p.submit();const r=p.requests[0];
  r.upload.onprogress({lengthComputable:true,loaded:256,total:512});assert.equal(p.nodes.get('firmware-progress').value,50);
  r.upload.onload();assert.match(p.nodes.get('firmware-status').textContent,/Verificando/);
  r.status=200;r.responseText='Firmware verificado';r.onload();assert.equal(p.nodes.get('firmware-progress').value,100);
  assert.equal(p.nodes.get('firmware-send').disabled,true);
});
test('network failure reports uncertain outcome and allows selecting another file', () => {
  const p=page();p.submit();p.requests[0].onerror();
  assert.match(p.nodes.get('firmware-status').textContent,/Confira a serial/);
  assert.equal(p.nodes.get('firmware-send').disabled,false);
  assert.equal(p.nodes.get('firmware-file').disabled,false);
});
test('server rejection releases upload controls and shows the supplied reason', () => {
  const p=page();p.submit();const r=p.requests[0];r.status=400;r.responseText='Transferencia incompleta';r.onload();
  assert.equal(p.nodes.get('firmware-status').textContent,'Transferencia incompleta');
  assert.equal(p.nodes.get('firmware-send').disabled,false);
});
