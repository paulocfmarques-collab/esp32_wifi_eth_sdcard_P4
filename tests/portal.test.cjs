const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '..', 'PortalUi.h'), 'utf8');
const consoleHtml = source.match(/R"CONSOLE\(([\s\S]*?)\)CONSOLE"/)[1];
const script = consoleHtml.match(/<script>([\s\S]*?)<\/script>/)[1];

function browser({ fail = false, approve = true } = {}) {
  const nodes = new Map();
  for (const id of consoleHtml.matchAll(/id='([^']+)'/g)) {
    nodes.set(id[1], { textContent: '', value: '', className: '', disabled: false,
      focus() {}, querySelectorAll() { return []; }, replaceWith(next) { this.updated = next; } });
  }
  nodes.set('dashboard', { replaceWith(next) { this.updated = next; } });
  const requests = [], intervals = [];
  const document = { hidden: false, getElementById: id => nodes.get(id), addEventListener() {} };
  vm.runInNewContext(script, {
    document, TextEncoder, URLSearchParams, AbortSignal, Date,
    confirm: () => approve,
    setInterval: fn => intervals.push(fn),
    DOMParser: class { parseFromString() { return { getElementById: () => ({ textContent: 'new data' }) }; } },
    fetch: async (url, options) => {
      requests.push({ url, options });
      if (fail) throw Error('offline');
      return { ok: true, status: 200, text: async () => url.includes('command') ? 'ALIVE\n' : '<section id="dashboard"></section>' };
    }
  });
  return { nodes, requests, document, refresh: intervals[0],
    async send(command) {
      nodes.get('command-input').value = command;
      nodes.get('command-form').onsubmit({ preventDefault() {} });
      await new Promise(resolve => setImmediate(resolve));
    } };
}

test('all embedded scripts have valid JavaScript syntax', () => {
  for (const match of source.matchAll(/<script>([\s\S]*?)<\/script>/g)) new vm.Script(match[1]);
});
test('command POST carries the header, preserves argument spaces, and prints response', async () => {
  const b = browser(); await b.send('alive');
  const request = b.requests.find(r => r.url === '/api/command');
  assert.equal(request.options.headers['X-ESP32-Command'], '1');
  assert.equal(request.options.body.get('cmd'), 'alive');
  assert.match(b.nodes.get('console-output').textContent, /ALIVE/);
  assert.equal(b.nodes.get('command-input').value, '');
});
test('oversized UTF-8 input and embedded newline never reach the server', async () => {
  const b = browser(); await b.send('é'.repeat(128)); await b.send('alive\nreboot');
  assert.equal(b.requests.length, 0);
});
test('web automatic pages shortcut resumes the OLED rotation', async () => {
  const b = browser();
  b.nodes.get('quick-commands').onclick({ target: { closest: () => ({ dataset: { command: 'tela:auto' } }) } });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(b.requests.find(r => r.url === '/api/command').options.body.get('cmd'), 'tela:auto');
  assert.match(source, /data-command='tela:auto'>Telas automáticas/);
});
test('destructive commands require confirmation', async () => {
  const b = browser({ approve: false }); await b.send('sd_del:/a.txt'); await b.send('restart'); await b.send('reset_counters');
  assert.equal(b.requests.length, 0);
});

test('robot screensaver shortcut sends the command through the shared endpoint', async () => {
  const b = browser();
  b.nodes.get('quick-commands').onclick({ target: { closest: () => ({ dataset: { command: 'protetor:on' } }) } });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(b.requests.find(r => r.url === '/api/command').options.body.get('cmd'), 'protetor:on');
  assert.match(source, /data-command='protetor:on'>Carinhas de robô/);
});

test('web next-page command is available to exit the robot and resume navigation', async () => {
  const b = browser(); await b.send('tela:next');
  assert.equal(b.requests.find(r => r.url === '/api/command').options.body.get('cmd'), 'tela:next');
  assert.match(source, /data-command='tela:next'>Próxima tela/);
});

test('audio test shortcut requests a diagnostic tone without changing pages', async () => {
  const b = browser();
  b.nodes.get('quick-commands').onclick({ target: { closest: () => ({ dataset: { command: 'audio_test' } }) } });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(b.requests.find(r => r.url === '/api/command').options.body.get('cmd'), 'audio_test');
  assert.equal(b.requests.filter(r => r.url === '/api/command').length, 1);
  assert.match(source, /data-command='audio_test'>Testar som/);
});
test('crash_info is not sent when archiving confirmation is rejected', async () => {
  const b = browser({ approve: false }); await b.send('crash_info');
  assert.equal(b.requests.length, 0);
});
test('confirmed crash_info is sent through the shared command endpoint', async () => {
  const b = browser(); await b.send('crash_info');
  assert.equal(b.requests.find(r => r.url === '/api/command').options.body.get('cmd'), 'crash_info');
});
test('Wi-Fi passwords never appear in terminal or command history', async () => {
  const b = browser(); await b.send('wifi_add:Rede| segredo ');
  assert.equal(b.requests[0].options.body.get('cmd'), 'wifi_add:Rede| segredo ');
  assert.doesNotMatch(b.nodes.get('console-output').textContent, /segredo/);
  b.nodes.get('command-input').onkeydown({ key: 'ArrowUp', preventDefault() {} });
  assert.equal(b.nodes.get('command-input').value, '');
});
test('dashboard refresh replaces only dashboard and preserves typed command', async () => {
  const b = browser(); b.nodes.get('command-input').value = 'sd_tail:/log.txt:10';
  await b.refresh();
  assert.ok(b.nodes.get('dashboard').updated);
  assert.equal(b.nodes.get('command-input').value, 'sd_tail:/log.txt:10');
  b.nodes.get('pause-live').onclick(); await b.refresh();
  assert.equal(b.requests.length, 1);
});
test('failed command reports uncertain execution and reenables input', async () => {
  const b = browser({ fail: true }); await b.send('led_on');
  assert.match(b.nodes.get('console-output').textContent, /pode ter sido executado/);
  assert.equal(b.nodes.get('command-input').value, '');
  await b.send('status'); assert.equal(b.requests.filter(r => r.url === '/api/command').length, 2);
});

test('weather shortcut requests the external climate diagnostic', async () => {
  const b = browser();
  b.nodes.get('quick-commands').onclick({ target: { closest: () => ({ dataset: { command: 'weather_status' } }) } });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(b.requests.find(r => r.url === '/api/command').options.body.get('cmd'), 'weather_status');
  assert.match(source, /data-command='weather_status'>Clima externo/);
});

test('camera OLED shortcut uses the protected command endpoint', async () => {
 const b=browser();b.nodes.get('quick-commands').onclick({target:{closest:()=>({dataset:{command:'camera_oled'}})}});
 await new Promise(resolve=>setImmediate(resolve));
 assert.equal(b.requests.find(r=>r.url==='/api/command').options.body.get('cmd'),'camera_oled');
 assert.match(source,/data-command='camera_oled'>Câmera no OLED/);
});
