const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '..', 'PortalUi.h'), 'utf8');
const html = source.match(/R"USBVIEW\(([\s\S]*?)\)USBVIEW"/)[1];
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const tick = () => new Promise(resolve => setImmediate(resolve));
async function browser({ fail = false, pending = false } = {}) {
  const nodes = new Map([...html.matchAll(/id='([^']+)'/g)].map(m => [m[1], { disabled: false, textContent: '' }]));
  const requests = [], doc = { hidden: false, getElementById: id => nodes.get(id) };
  let refresh;
  vm.runInNewContext(script, { document: doc, AbortSignal, URLSearchParams,
    setInterval: fn => { refresh = fn; },
    fetch: async (url, options) => {
      requests.push({ url, options }); if (pending) await new Promise(() => {});
      if (fail) throw Error('offline');
      return { ok: true, text: async () => url.includes('command') ? 'Releitura solicitada' : 'FILE <script>bad</script> 42 B',
        json: async () => url.includes('/log/') ? { queued: 3, written: 22, dropped: 1 } : {
          state: 1, keyboard: true, mouse: true, keyboard_reports: 5, mouse_reports: 10,
          drive_connected: true, fat_mounted: true, drive_bytes: 1000000, errors: 0 } };
    } });
  await tick();
  return { requests, doc, refresh, el: key => nodes.get('usb-' + key) };
}
test('USB page displays host, HID, FAT and logger state', async () => {
  const b = await browser(); assert.equal(b.requests.length, 3);
  assert.match(b.el('status').textContent, /Host ativo/); assert.match(b.el('status').textContent, /FAT: montado/);
  assert.match(b.el('logs').textContent, /descartados: 1/);
});
test('USB filenames are rendered as text instead of executable markup', async () => {
  const b = await browser(); assert.match(b.el('files').textContent, /<script>bad<\/script>/);
  assert.equal(b.el('files').innerHTML, undefined);
});
test('USB root rescan uses protected POST and reports its asynchronous result', async () => {
  const b = await browser(); await b.el('rescan').onclick();
  const r = b.requests.find(r => r.url === '/api/command');
  assert.equal(r.options.headers['X-ESP32-Command'], '1'); assert.equal(r.options.body.get('cmd'), 'usb_rescan');
  assert.match(b.el('message').textContent, /Releitura solicitada/); assert.equal(b.el('rescan').disabled, false);
});
test('USB polling pauses while hidden', async () => {
  const b = await browser(); b.doc.hidden = true; await b.refresh(); assert.equal(b.requests.length, 3);
});
test('USB polling never overlaps a pending refresh', async () => {
  const b = await browser({ pending: true }); await b.refresh(); assert.equal(b.requests.length, 3);
});
test('USB failures identify stale data and release controls', async () => {
  const b = await browser({ fail: true }); assert.match(b.el('message').textContent, /desatualizados/);
  assert.equal(b.el('refresh').disabled, false); await b.el('rescan').onclick();
  assert.match(b.el('message').textContent, /Não foi possível confirmar/);
});
