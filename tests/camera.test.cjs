const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '..', 'PortalUi.h'), 'utf8');
const html = source.match(/R"CAMERA\(([\s\S]*?)\)CAMERA"/)[1];
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const tick = () => new Promise(resolve => setImmediate(resolve));
async function browser({ wrongType = false, hold = false, port = 81, available = true } = {}) {
 const nodes = new Map([...html.matchAll(/id='([^']+)'/g)].map(m => [m[1], {
  src: '', href: '', disabled: false, textContent: '', removeAttribute(key) { this[key] = ''; }
 }]));
 const requests = [], revoked = [], callbacks = {};
 let refresh, counter = 0, bad = false;
 const document = { hidden: false, getElementById: id => nodes.get(id), addEventListener: (n, fn) => { callbacks[n] = fn; } };
 class BrowserURL extends URL {
  static createObjectURL() { return 'blob:' + ++counter; }
  static revokeObjectURL(url) { revoked.push(url); }
 }
 vm.runInNewContext(script, { document, AbortSignal, URL: BrowserURL,
  window: { location: { href: 'http://192.168.0.125/camera' }, addEventListener: (n, fn) => { callbacks[n] = fn; } },
  setInterval: fn => { refresh = fn; },
  fetch: async (url, options) => {
   requests.push({ url, options }); if (hold) await new Promise(() => {});
   return { ok: !bad, text: async () => 'Camera sem imagem recente', json: async () => ({ stream_port: port, available }),
    blob: async () => ({ type: wrongType ? 'text/html' : 'image/jpeg', size: 1234 }) };
  } });
 await tick();
 return { document, requests, revoked, refresh, callbacks, setFailure: value => { bad = value; },
  el: key => nodes.get('camera-' + key), async photo() { await nodes.get('camera-refresh').onclick(); } };
}
test('camera starts MJPEG using the port supplied by the device', async () => {
 const b=await browser({port:82});assert.equal(b.requests[0].url,'/api/camera/status');
 assert.match(b.el('image').src,/^http:\/\/192\.168\.0\.125:82\/stream\?t=/);
 assert.equal(b.el('url').textContent,'http://192.168.0.125:82/stream');b.el('image').onload();assert.match(b.el('result').textContent,/Vídeo MJPEG ativo/);
});
test('camera pause closes the video and resume starts it again', async () => {
 const b=await browser();b.el('pause').onclick();await b.refresh();await tick();assert.equal(b.el('image').src,'');assert.equal(b.requests.length,1);
 b.el('pause').onclick();await tick();assert.match(b.el('image').src,/\/stream/);
});
test('hidden camera page closes the stream and resumes when visible', async () => {
 const b=await browser();b.document.hidden=true;b.callbacks.visibilitychange();await b.refresh();await tick();assert.equal(b.el('image').src,'');assert.equal(b.requests.length,1);
 b.document.hidden=false;b.callbacks.visibilitychange();await tick();assert.equal(b.requests.length,2);
});
test('camera snapshots replace and release object URLs', async () => {
 const b=await browser();await b.photo();assert.equal(b.el('image').src,'blob:1');assert.equal(b.requests[1].url,'/camera.jpg');
 assert.equal(b.requests[1].options.cache,'no-store');await b.refresh();await tick();assert.equal(b.el('image').src,'blob:2');assert.deepEqual(b.revoked,['blob:1']);
});
test('snapshot failure preserves old picture with a stale-image warning', async () => {
 const b=await browser();await b.photo();b.setFailure(true);await b.refresh();await tick();assert.equal(b.el('image').src,'blob:1');assert.match(b.el('result').textContent,/desatualizada/);assert.equal(b.el('refresh').disabled,false);
});
test('camera rejects non-JPEG snapshot responses', async () => {
 const b=await browser({wrongType:true});await b.photo();assert.equal(b.el('image').src,'');assert.match(b.el('result').textContent,/sem imagem JPEG/);
});
test('camera prevents overlapping requests during status lookup', async () => {
 const b=await browser({hold:true});await b.refresh();await b.photo();assert.equal(b.requests.length,1);
});
test('camera exit releases snapshot and closes displayed source', async () => {
 const b=await browser();await b.photo();b.callbacks.pagehide();assert.deepEqual(b.revoked,['blob:1']);assert.equal(b.el('image').src,'');
});
test('unavailable stream or sensor reports useful errors', async () => {
 const b=await browser({port:0});assert.equal(b.el('image').src,'');assert.match(b.el('result').textContent,/Servidor de vídeo indisponível/);
 const c=await browser({available:false});assert.equal(c.el('image').src,'');assert.match(c.el('result').textContent,/sem imagem recente/);
});
