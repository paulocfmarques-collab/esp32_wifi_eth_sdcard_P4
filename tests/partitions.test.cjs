const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const partitions = fs.readFileSync(path.join(__dirname, '..', 'partitions.csv'), 'utf8')
  .split(/\r?\n/).filter(line => line.trim() && !line.trim().startsWith('#'))
  .map(line => { const [name, type, subtype, offset, size] = line.split(',').map(s => s.trim());
    return { name, type, subtype, offset: Number(offset), size: Number(size) }; });
const find = name => partitions.find(p => p.name === name);
test('partition table stays within 16 MiB without overlapping bootloader or partition table', () => {
  let end = 0x9000;
  for (const p of partitions) {
    assert.ok(Number.isInteger(p.offset) && Number.isInteger(p.size) && p.size > 0);
    assert.ok(p.offset >= end, `${p.name} overlaps previous partition`);
    assert.equal(p.offset % (p.type === 'app' ? 0x10000 : 0x1000), 0);
    assert.equal(p.size % 0x1000, 0);
    end = p.offset + p.size;
  }
  assert.equal(end, 16 * 1024 * 1024);
});
test('OTA keeps two equal slots with distinct ota_0 and ota_1 subtypes', () => {
  const apps = partitions.filter(p => p.type === 'app');
  assert.equal(apps.length, 2); assert.deepEqual(apps.map(p => p.subtype), ['ota_0', 'ota_1']);
  assert.equal(apps[0].size, apps[1].size); assert.equal(apps[0].size, 6553600);
});
test('both OTA slots fit the reported 2316966-byte firmware with room to grow', () => {
  for (const app of partitions.filter(p => p.type === 'app')) assert.ok(app.size > 2316966 + 0x10000);
});
test('preferences and OTA metadata preserve their default addresses and sizes', () => {
  assert.equal(find('nvs').offset, 0x9000); assert.equal(find('nvs').size, 0x5000);
  assert.equal(find('otadata').offset, 0xe000); assert.equal(find('otadata').size, 0x2000);
});
test('core dump partition remains present and SPIFFS stops before it', () => {
  const dump = find('coredump'); assert.equal(dump.subtype, 'coredump');
  assert.equal(dump.size, 0x10000); assert.equal(find('spiffs').offset + find('spiffs').size, dump.offset);
});
