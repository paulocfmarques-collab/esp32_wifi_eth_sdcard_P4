const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '..', 'PortalUi.h'), 'utf8');
const html = source.match(/R"EDGE\(([\s\S]*?)\)EDGE"/)[1];
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const config = { enabled: false, sound: false, cpu_limit: 70, heap_limit: 65536,
  udp_limit: 5, confirm_seconds: 10, cooldown_seconds: 300 };
const state = { ...config, paused: false, rules: [
  { rule: 'cpu_hot', active: false, valid: true, pending: true },
  { rule: 'heap_low', active: true, valid: true, pending: false },
  { rule: 'network_down', active: false, valid: false, pending: false }],
  history: [{ sequence: 1, uptime: 70, rule: 'heap_low', active: true }] };
const tick = () => new Promise(resolve => setImmediate(resolve));
async function browser({ loadFailure = false, saveFailure = false, holdSave = false, statusFailure = false } = {}) {
  const nodes = new Map([...html.matchAll(/id='([^']+)'/g)].map(m =>
    [m[1], { value: '', checked: false, disabled: m[1] === 'edge-fields', textContent: '' }]));
  const requests = [], document = { hidden: false, getElementById: id => nodes.get(id) };
  let refresh;
  vm.runInNewContext(script, { document, URLSearchParams, AbortSignal,
    setInterval: fn => { refresh = fn; },
    fetch: async (url, options = {}) => {
      requests.push({ url, options });
      if (url.endsWith('/status') && statusFailure) throw Error('offline');
      if (url.endsWith('/config') && !options.method && loadFailure) throw Error('offline');
      if (options.method && holdSave) await new Promise(() => {});
      return { ok: !(options.method && saveFailure), json: async () => url.endsWith('/status') ? state : config,
        text: async () => 'NVS indisponivel' };
    } });
  await tick();
  return { requests, document, refresh, el: key => nodes.get('edge-' + key),
    submit: () => nodes.get('edge-form').onsubmit({ preventDefault() {} }) };
}
test('edge loads saved limits and displays pending, active and unavailable rules', async () => {
  const b = await browser();
  assert.equal(b.el('fields').disabled, false); assert.equal(b.el('cpu_limit').value, 70);
  assert.match(b.el('status').textContent, /confirmando mudança/);
  assert.match(b.el('status').textContent, /Memória baixa: ALERTA/);
  assert.match(b.el('status').textContent, /aguardando dados/);
  assert.match(b.el('history').textContent, /70 s ligado/);
});
test('edge saves all thresholds, activation and sound with cross-origin protection header', async () => {
  const b = await browser(); b.el('enabled').checked = true; b.el('sound').checked = true;
  await b.submit();
  const r = b.requests.find(r => r.options.method === 'POST');
  assert.equal(r.url, '/api/edge/config');
  assert.equal(r.options.headers['X-ESP32-Command'], '1');
  assert.equal(r.options.body.get('enabled'), '1'); assert.equal(r.options.body.get('sound'), '1');
  assert.equal(r.options.body.get('confirm_seconds'), '10');
  assert.equal(r.options.body.get('heap_limit'), '65536');
});
test('edge rejects malformed, fractional and out-of-range thresholds without saving', async () => {
  const b = await browser();
  for (const [key, values] of Object.entries({ cpu_limit: ['19', '101', '70.5', ''],
    heap_limit: ['8191', '1048577'], udp_limit: ['0', '10001'],
    confirm_seconds: ['0', '601', '-1'], cooldown_seconds: ['29', '86401', '3e2'] })) {
    const original = b.el(key).value;
    for (const value of values) { b.el(key).value = value; await b.submit(); }
    b.el(key).value = original;
  }
  assert.equal(b.requests.filter(r => r.options.method).length, 0);
});
test('edge accepts boundary limits and disables rules explicitly', async () => {
  const b = await browser(); b.el('enabled').checked = false;
  b.el('cpu_limit').value = '20'; b.el('heap_limit').value = '8192';
  b.el('udp_limit').value = '10000'; b.el('confirm_seconds').value = '600'; b.el('cooldown_seconds').value = '86400';
  await b.submit();
  assert.equal(b.requests.find(r => r.options.method).options.body.get('enabled'), '0');
});
test('edge status polling preserves thresholds being edited', async () => {
  const b = await browser(); b.el('cpu_limit').value = '82';
  await b.refresh(); assert.equal(b.el('cpu_limit').value, '82');
  const count = b.requests.length; b.document.hidden = true; await b.refresh();
  assert.equal(b.requests.length, count);
});
test('edge config load failure prevents overwriting unknown settings', async () => {
  const b = await browser({ loadFailure: true }); await b.submit();
  assert.equal(b.el('fields').disabled, true); assert.equal(b.requests.filter(r => r.options.method).length, 0);
});
test('edge reports failed saves and releases controls while keeping edits', async () => {
  const b = await browser({ saveFailure: true }); b.el('cpu_limit').value = '82'; await b.submit();
  assert.equal(b.el('save').disabled, false); assert.equal(b.el('cpu_limit').value, '82');
  assert.match(b.el('result').textContent, /NVS indisponivel/);
});
test('edge cannot overlap preference writes', async () => {
  const b = await browser({ holdSave: true }); b.submit(); await b.submit();
  assert.equal(b.requests.filter(r => r.options.method).length, 1); assert.equal(b.el('save').disabled, true);
});
test('edge status failures are visible instead of presenting old readings as current', async () => {
  const b = await browser({ statusFailure: true });
  assert.match(b.el('status').textContent, /desatualizados/);
});
