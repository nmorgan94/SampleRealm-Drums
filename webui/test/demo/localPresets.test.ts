import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { test } from 'node:test';
import { instantiate } from '../../src/demo/engine.ts';
import { parsePreset, STORAGE_KEY, type PresetStorage } from '../../src/demo/localPresets.ts';
import { workletBackend } from '../../src/demo/workletBackend.ts';
import { info, wasm } from '../engine.ts';

const presetFolder = resolve(import.meta.dirname, '../../../Assets/Presets');
const factory = readdirSync(presetFolder)
  .filter((file) => file.endsWith('.srdrum'))
  .map((file) => parsePreset(readFileSync(resolve(presetFolder, file), 'utf8')));

// By category, then name, as PresetManager sorts them
const KICKS = ['Neuro Kick', 'Rolling Kick', 'Tight Punch', 'Liquid Kick', 'Deep Sub', 'Jungle Kick'];
const SNARES = ['Halftime Snare', 'Liquid Snare', 'Neuro Snare', 'Rolling Snare', 'Snap Snare', 'Jungle Snare'];

const fakeStorage = (): PresetStorage & { items: Map<string, string> } => {
  const items = new Map<string, string>();
  return { items, getItem: (key) => items.get(key) ?? null, setItem: (key, value) => void items.set(key, value) };
};

const setUp = (storage = fakeStorage()) => {
  const backend = workletBackend(info, instantiate(wasm), factory, storage);
  return { backend, presets: backend.presets, storage };
};

const named = <T extends { name: string }>(list: T[], name: string) => {
  const found = list.find((preset) => preset.name === name);
  assert.ok(found, `No preset ${name}`);
  return found;
};

const names = async (presets: { list(): Promise<{ name: string }[]> }) => (await presets.list()).map((preset) => preset.name);

test('factory files are read with their category, type, values and envelopes', () => {
  const jungle = named(factory, 'Jungle Kick');

  assert.equal(factory.length, 18);
  assert.deepEqual({ ...jungle, envelopes: Object.keys(jungle.envelopes) }, {
    name: 'Jungle Kick', category: 'Vintage', type: 'Kick', isFactory: true,
    values: { instrument: 0, subHarmonics: 30, driveType: 3, driveAmount: 35, eqHighGain: -3, outputGain: -4 },
    envelopes: ['pitch', 'amp'],
  });
  assert.deepEqual(jungle.envelopes.pitch![0], { time: 0, value: 2500, curve: 0 });
});

test('only the current instrument\'s presets are listed, by category then name', async () => {
  const { backend, presets } = setUp();
  assert.deepEqual(await names(presets), KICKS);

  backend.setValue('instrument', 1);
  assert.deepEqual(await names(presets), SNARES);
});

test('a loaded preset sets its values and envelopes, the rest at their defaults', async () => {
  const { backend, presets } = setUp();
  backend.setValue('tune', 5);
  backend.setEnvelope(1, [{ time: 0, value: 1, curve: 0 }, { time: 0.05, value: 0, curve: 0 }]);
  backend.setEnvelope(5, [{ time: 0, value: 1, curve: 0 }, { time: 1, value: 0, curve: 0 }]);

  await presets.load(named(await presets.list(), 'Jungle Kick'));

  assert.equal(backend.getValue('driveType'), 3);
  assert.equal(backend.getValue('tune'), 0);
  assert.equal(backend.getEnvelope(1).at(-1)!.time, 0.2);
  assert.deepEqual(backend.getEnvelope(5), info.envelopes[5]!.nodes);
  assert.deepEqual(presets.status(), { current: { name: 'Jungle Kick', category: 'Vintage', isFactory: true }, modified: false });
});

test('editing an envelope marks the preset modified until the next load', async () => {
  const { backend, presets } = setUp();
  await presets.load(named(await presets.list(), 'Deep Sub'));

  backend.setEnvelope(0, info.envelopes[0]!.nodes);
  assert.equal(presets.status().modified, true);

  await presets.loadNext();
  assert.equal(presets.status().modified, false);
});

test('Init keeps the instrument and restores everything else', async () => {
  const { backend, presets } = setUp();
  backend.setValue('instrument', 1);
  await presets.load(named(await presets.list(), 'Neuro Snare'));

  await presets.loadInit();

  assert.equal(backend.getValue('instrument'), 1);
  assert.equal(backend.getValue('driveAmount'), 20);
  assert.deepEqual(backend.getEnvelope(2), info.envelopes[2]!.nodes);
  assert.equal(presets.status().current.name, 'Init');
});

test('next and previous wrap within the instrument, and start from either end after Init', async () => {
  const { presets } = setUp();

  await presets.loadNext();
  assert.equal(presets.status().current.name, KICKS[0]);
  await presets.loadPrevious();
  assert.equal(presets.status().current.name, KICKS.at(-1));

  await presets.loadInit();
  await presets.loadPrevious();
  assert.equal(presets.status().current.name, KICKS.at(-1));
});

test('user presets save with their instrument and envelopes, rename and delete, and outlast the page', async () => {
  const storage = fakeStorage();
  const { backend, presets } = setUp(storage);
  const shortAmp = [{ time: 0, value: 1, curve: 0 }, { time: 0.05, value: 0, curve: 0 }];
  backend.setEnvelope(1, shortAmp);

  assert.equal(await presets.save('Mine', 'User'), null);
  assert.deepEqual(presets.status(), { current: { name: 'Mine', category: 'User', isFactory: false }, modified: false });
  assert.ok(storage.items.get(STORAGE_KEY)?.includes('"type":"Kick"'));

  const reopened = setUp(storage);
  await reopened.presets.load(named(await reopened.presets.list(), 'Mine'));
  assert.deepEqual(reopened.backend.getEnvelope(1), shortAmp);

  // Another instrument doesn't list it
  reopened.backend.setValue('instrument', 2);
  assert.ok(!(await names(reopened.presets)).includes('Mine'));
  reopened.backend.setValue('instrument', 0);

  assert.equal(await presets.rename(named(await presets.list(), 'Mine'), 'Ours'), null);
  assert.equal(presets.status().current.name, 'Ours');
  assert.equal(await presets.remove(named(await presets.list(), 'Ours')), null);
  assert.ok(!(await names(presets)).includes('Ours'));
});

test('user presets can\'t take a factory name', async () => {
  const { presets } = setUp();
  assert.match((await presets.save('jungle kick', ''))!, /factory preset/);
});

test('stored envelopes that are broken load at their defaults', async () => {
  const storage = fakeStorage();
  storage.setItem(STORAGE_KEY, JSON.stringify([{ name: 'Odd', category: '', type: 'Kick', values: { tune: 2 }, envelopes: { amp: [{ time: 0, value: 1, curve: 0 }] } }]));
  const { backend, presets } = setUp(storage);

  await presets.load(named(await presets.list(), 'Odd'));

  assert.equal(backend.getValue('tune'), 2);
  assert.deepEqual(backend.getEnvelope(1), info.envelopes[1]!.nodes);
});

test('unreadable storage leaves only the factory presets', async () => {
  const storage = fakeStorage();
  storage.setItem(STORAGE_KEY, 'not json');
  assert.deepEqual(await names(setUp(storage).presets), KICKS);
});
