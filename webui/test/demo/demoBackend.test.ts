// What Drums adds to the shared worklet backend: envelopes, hits, the preview and presets typed by instrument.
// The rest is tested in SampleRealm-Modules
import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { test } from 'node:test';
import { parsePreset } from '@samplerealm/modules/demo/localPresets.ts';
import { parseState, type EnginePort } from '@samplerealm/modules/demo/workletBackend.ts';
import { memoryStorage } from '@samplerealm/modules/testing/presets.ts';
import { demoBackend, parseEnvelopes, STORAGE_KEY } from '../../src/demo/demoBackend.ts';
import { flattenNodes } from '../../src/demo/engine.ts';
import { drums, engine, info } from '../engine.ts';

const presetFolder = resolve(import.meta.dirname, '../../../Assets/Presets');
const factoryPresets = readdirSync(presetFolder)
  .filter((file) => file.endsWith('.srdrum'))
  .map((file) => readFileSync(resolve(presetFolder, file), 'utf8'));

// By category, then name, as PresetManager sorts them
const KICKS = ['Neuro Kick', 'Rolling Kick', 'Tight Punch', 'Liquid Kick', 'Deep Sub', 'Jungle Kick'];
const SNARES = ['Halftime Snare', 'Liquid Snare', 'Neuro Snare', 'Rolling Snare', 'Snap Snare', 'Jungle Snare'];

const fakePort = () => {
  const posted: unknown[] = [];
  const port: EnginePort = { postMessage: (message: unknown) => void posted.push(message), onmessage: null };
  const receive = (data: unknown) => port.onmessage?.(new MessageEvent('message', { data }));
  return { port, posted, receive };
};

// The page's preview engine is never prepared by the worklet
const setUp = (storage = memoryStorage()) => {
  const backend = demoBackend(info, engine(null).engine, { factoryPresets, storage });
  return { backend, presets: backend.presets, storage };
};

const named = <T extends { name: string }>(list: T[], name: string) => {
  const found = list.find((preset) => preset.name === name);
  assert.ok(found, `No preset ${name}`);
  return found;
};

const names = async (presets: { list(): Promise<{ name: string }[]> }) => (await presets.list()).map((preset) => preset.name);

const indexOf = (id: string) => info.params.findIndex((param) => param.id === id);
const shortAmp = [{ time: 0, value: 1, curve: 0 }, { time: 0.05, value: 0, curve: 0 }];

//==============================================================================
test('changes made before the worklet attaches reach it once it does, in order and once each', () => {
  const { backend } = setUp();
  backend.setValue('tune', -12);
  backend.setEnvelope(1, shortAmp);
  backend.noteOn(36);
  backend.audition(0.5);

  const { port, posted } = fakePort();
  backend.attach(port);
  backend.setValue('tune', 2);

  assert.deepEqual(posted, [
    { type: 'param', index: indexOf('tune'), value: -12 },
    { type: 'call', name: 'setEnvelope', args: [1, 2], floats: { at: 'envelopeNodes', values: flattenNodes(shortAmp) } },
    { type: 'midi', bytes: [0x90, 36, 100] },
    { type: 'call', name: 'audition', args: [0.5] },
    { type: 'param', index: indexOf('tune'), value: 2 },
  ]);
});

test('envelopes read back and notify before the worklet attaches', () => {
  const { backend } = setUp();
  const shapes: number[] = [];
  backend.subscribeEnvelope(1, (nodes) => shapes.push(nodes.length));

  assert.deepEqual(backend.getEnvelope(1), drums.envelopes[1]!.nodes);

  backend.setEnvelope(1, shortAmp);

  assert.deepEqual(backend.getEnvelope(1), shortAmp);
  assert.deepEqual(shapes, [2]);
  assert.equal(backend.presets.status().modified, true);
});

test('the preview follows the values and envelopes', async () => {
  const { backend } = setUp();
  const kick = await backend.preview(29, 8);
  assert.ok(Math.abs(kick.tailHz - 43.65) < 0.01);

  backend.setEnvelope(1, shortAmp);
  backend.setValue('tune', 12);
  const shorter = await backend.preview(29, 8);

  assert.ok(shorter.samples.length < kick.samples.length / 2);
  assert.ok(Math.abs(shorter.tailHz - 87.31) < 0.01);
});

test('each instrument keeps the last note it played, starting from its default', () => {
  const { backend } = setUp();
  const { port, receive } = fakePort();
  backend.attach(port);
  const hits: number[][] = [];
  backend.onHit((note, level, velocity) => hits.push([note, level, velocity]));

  assert.equal(backend.lastNote(), 29);
  receive({ type: 'watch', values: { hitCount: 1, lastNote: 33, lastHitLevel: 0.5, lastHitVelocity: 0.25 } });
  assert.equal(backend.lastNote(), 33);

  // The worklet's last note follows the instrument, which isn't a hit
  backend.setValue('instrument', 1);
  receive({ type: 'watch', values: { hitCount: 1, lastNote: 55 } });
  assert.equal(backend.lastNote(), 55);

  backend.setValue('instrument', 0);
  assert.equal(backend.lastNote(), 33);
  assert.deepEqual(hits, [[33, 0.5, 0.25]]);
});

test('the loop is switched in the worklet and read back here', () => {
  const { backend } = setUp();
  const { port, posted } = fakePort();
  backend.attach(port);

  assert.deepEqual(backend.patterns, ['Two-step', 'Halftime', 'Four-floor', 'Sixteenths']);
  assert.deepEqual(backend.getLoop(), { pattern: 0, on: false });

  backend.setLoop({ pattern: 2, on: true });
  assert.deepEqual(backend.getLoop(), { pattern: 2, on: true });
  assert.deepEqual(posted.at(-1), { type: 'call', name: 'setLoop', args: [2, 1] });
});

test("the loop's steps are switched in the worklet and read back here, with the step playing", () => {
  const { backend } = setUp();
  const { port, posted, receive } = fakePort();
  backend.attach(port);

  const seen: { steps: number[]; playing: number }[] = [];
  backend.subscribeLoopSteps((steps) => seen.push(steps));

  assert.deepEqual(backend.getLoopSteps(), { steps: [127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 112, 0, 0, 0, 0, 0], playing: -1 });

  const edited = [127, 0, 0, 0, 64, 0, 0, 0, 0, 0, 112, 0, 0, 0, 0, 0];
  backend.setLoopStep(4, 64);
  assert.deepEqual(posted.at(-1), { type: 'call', name: 'setLoopStep', args: [4, 64] });
  assert.deepEqual(seen.at(-1), { steps: edited, playing: -1 });

  receive({ type: 'watch', values: { hitCount: 0, lastNote: 29, lastHitLevel: 0, lastHitVelocity: 0, playingStep: 3 } });
  assert.deepEqual(seen.at(-1), { steps: edited, playing: 3 });

  // The snare's own steps, then every instrument's from the new pattern
  backend.setValue('instrument', 1);
  assert.deepEqual(seen.at(-1)?.steps, [0, 0, 0, 0, 127, 0, 0, 0, 0, 0, 0, 0, 127, 0, 0, 0]);

  backend.setValue('instrument', 0);
  backend.setLoop({ pattern: 1, on: false });
  assert.deepEqual(seen.at(-1)?.steps, [127, 0, 0, 0, 0, 0, 90, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
});

test('values read as the plugin shows them', () => {
  const { backend } = setUp();

  assert.equal(backend.formatValue(backend.param('clickTone'), 1500), '1.50 kHz');
});

//==============================================================================
test('factory files are read with their category, type, values and envelopes', () => {
  const parsed = factoryPresets.map((xml) => parsePreset(xml, (text) => ({ ...parseState(text), envelopes: parseEnvelopes(text) })));
  const jungle = named(parsed, 'Jungle Kick');

  assert.equal(parsed.length, 18);
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
  backend.setEnvelope(1, shortAmp);
  backend.setEnvelope(5, [{ time: 0, value: 1, curve: 0 }, { time: 1, value: 0, curve: 0 }]);

  await presets.load(named(await presets.list(), 'Jungle Kick'));

  assert.equal(backend.getValue('driveType'), 3);
  assert.equal(backend.getValue('tune'), 0);
  assert.equal(backend.getEnvelope(1).at(-1)!.time, 0.2);
  assert.deepEqual(backend.getEnvelope(5), drums.envelopes[5]!.nodes);
  assert.deepEqual(presets.status(), { current: { name: 'Jungle Kick', category: 'Vintage', isFactory: true }, modified: false });
});

test('Init keeps the instrument and restores everything else', async () => {
  const { backend, presets } = setUp();
  backend.setValue('instrument', 1);
  await presets.load(named(await presets.list(), 'Neuro Snare'));

  await presets.loadInit();

  assert.equal(backend.getValue('instrument'), 1);
  assert.equal(backend.getValue('driveAmount'), 20);
  assert.deepEqual(backend.getEnvelope(2), drums.envelopes[2]!.nodes);
  assert.equal(presets.status().current.name, 'Init');
});

test('user presets save with their instrument and envelopes, and outlast the page', async () => {
  const storage = memoryStorage();
  const { backend, presets } = setUp(storage);
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
});

// As the demo stored them before it shared its presets' code
test('presets stored before still load, and broken envelopes take their defaults', async () => {
  const stored = [{ name: 'Odd', category: '', type: 'Kick', values: { tune: 2 }, envelopes: { amp: [{ time: 0, value: 1, curve: 0 }], pitch: shortAmp.map((node) => ({ ...node, value: 100 - 50 * node.time })) } }];
  const { backend, presets } = setUp(memoryStorage({ [STORAGE_KEY]: JSON.stringify(stored) }));

  await presets.load(named(await presets.list(), 'Odd'));

  assert.equal(backend.getValue('tune'), 2);
  assert.deepEqual(backend.getEnvelope(1), drums.envelopes[1]!.nodes);
  assert.equal(backend.getEnvelope(0).length, 2);
  assert.equal(backend.getEnvelope(0)[0]!.value, 100);
});
