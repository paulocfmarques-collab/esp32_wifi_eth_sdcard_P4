const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '..', 'PortalUi.h'), 'utf8');
const html = source.match(/R"MQTT\(([\s\S]*?)\)MQTT"/)[1];
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const config = { enabled: true, host: '192.168.0.10', port: 1883, user: 'esp32',
  password_set: true, connected: false, base_topic: 'esp32/esp32p4_test' };
const tick = () => new Promise(resolve => setImmediate(resolve));
async function browser({ loadFailure = false, saveFailure = false, holdSave = false } = {}) {
  const nodes = new Map([...html.matchAll(/id='([^']+)'/g)].map(m =>
    [m[1], { value: '', checked: false, disabled: m[1] === 'mqtt-fields', textContent: '' }]));
  const requests = [];
  vm.runInNewContext(script, { document: { getElementById: id => nodes.get(id) },
    URLSearchParams, AbortSignal,
    fetch: async (url, options = {}) => {
      requests.push({ url, options });
      if (!options.method && loadFailure) throw Error('offline');
      if (options.method && holdSave) await new Promise(() => {});
      return { ok: !(options.method && saveFailure), json: async () => config,
        text: async () => 'NVS indisponivel' };
    } });
  await tick();
  return { nodes, requests, el: key => nodes.get('mqtt-' + key),
    submit: () => nodes.get('mqtt-form').onsubmit({ preventDefault() {} }) };
}

test('MQTT loads saved settings without exposing a stored password', async () => {
  const b = await browser();
  assert.equal(b.el('host').value, config.host);
  assert.equal(b.el('password').value, '');
  assert.equal(b.el('fields').disabled, false);
  assert.match(b.el('result').textContent, /Senha salva/);
});
test('MQTT save preserves an omitted password and carries the protected POST header', async () => {
  const b = await browser(); await b.submit();
  const r = b.requests[1];
  assert.equal(r.url, '/api/mqtt/config');
  assert.equal(r.options.headers['X-ESP32-Command'], '1');
  assert.equal(r.options.body.get('password'), '');
  assert.equal(r.options.body.get('clear_password'), '0');
  assert.equal(r.options.body.get('enabled'), '1');
});
test('MQTT explicit password removal and disabling are submitted', async () => {
  const b = await browser(); b.el('clear').checked = true; b.el('enabled').checked = false;
  await b.submit();
  assert.equal(b.requests[1].options.body.get('clear_password'), '1');
  assert.equal(b.requests[1].options.body.get('enabled'), '0');
});
test('MQTT rejects URL hosts, missing enabled host and invalid ports', async () => {
  const b = await browser();
  for (const host of ['http://broker', 'broker/path', 'broker<script>', '']) {
    b.el('host').value = host; await b.submit();
  }
  b.el('host').value = 'broker.local';
  for (const port of ['0', '65536', '1883x', '']) { b.el('port').value = port; await b.submit(); }
  assert.equal(b.requests.length, 1);
});
test('failed MQTT load prevents overwriting unknown preferences', async () => {
  const b = await browser({ loadFailure: true }); await b.submit();
  assert.equal(b.el('fields').disabled, true); assert.equal(b.requests.length, 1);
});
test('MQTT save failure retains typed password for retry without displaying it', async () => {
  const b = await browser({ saveFailure: true }); b.el('password').value = 'private-secret';
  await b.submit();
  assert.equal(b.el('save').disabled, false);
  assert.equal(b.el('password').value, 'private-secret');
  assert.match(b.el('result').textContent, /NVS indisponivel/);
  assert.doesNotMatch(b.el('result').textContent, /private-secret/);
});
test('MQTT cannot overlap preference saves while a request is pending', async () => {
  const b = await browser({ holdSave: true }); b.submit(); await b.submit();
  assert.equal(b.requests.length, 2); assert.equal(b.el('save').disabled, true);
});
test('MQTT clears a newly entered password after a successful save', async () => {
  const b = await browser(); b.el('password').value = 'private-secret'; await b.submit();
  assert.equal(b.requests[1].options.body.get('password'), 'private-secret');
  assert.equal(b.el('password').value, '');
  assert.doesNotMatch(b.el('result').textContent, /private-secret/);
});
