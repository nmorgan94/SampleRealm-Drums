import assert from 'node:assert/strict';
import { test } from 'node:test';
import { flattenNodes, instantiate } from '../../src/demo/engine.ts';
import { workletBackend, type EnginePort } from '../../src/demo/workletBackend.ts';
import { info, wasm } from '../engine.ts';

const fakePort = () => {
  const posted: unknown[] = [];
  const port: EnginePort = { postMessage: (message: unknown) => void posted.push(message), onmessage: null };
  const receive = (data: unknown) => port.onmessage?.({ data } as MessageEvent);
  return { port, posted, receive };
};

const setUp = () => workletBackend(info, instantiate(wasm));

const indexOf = (id: string) => info.params.findIndex((param) => param.id === id);
const shortAmp = [{ time: 0, value: 1, curve: 0 }, { time: 0.05, value: 0, curve: 0 }];

test('changes made before the worklet attaches reach it once it does, in order and once each', () => {
  const backend = setUp();
  backend.setValue('tune', -12);
  backend.setEnvelope(1, shortAmp);
  backend.noteOn(36);
  backend.audition();

  const { port, posted } = fakePort();
  backend.attach(port);
  backend.setValue('tune', 2);

  assert.deepEqual(posted, [
    { type: 'param', index: indexOf('tune'), value: -12 },
    { type: 'envelope', index: 1, nodes: flattenNodes(shortAmp) },
    { type: 'midi', bytes: [0x90, 36, 100] },
    { type: 'audition' },
    { type: 'param', index: indexOf('tune'), value: 2 },
  ]);
});

test('values and envelopes read back and notify before the worklet attaches', () => {
  const backend = setUp();
  const heard: number[] = [];
  const shapes: number[] = [];
  backend.subscribe('tune', (value) => heard.push(value));
  backend.subscribeEnvelope(1, (nodes) => shapes.push(nodes.length));

  assert.equal(backend.getValue('tune'), 0);
  assert.deepEqual(backend.getEnvelope(1), info.envelopes[1]!.nodes);

  backend.setValue('tune', -6);
  backend.setEnvelope(1, shortAmp);

  assert.equal(backend.getValue('tune'), -6);
  assert.deepEqual(heard, [-6]);
  assert.deepEqual(backend.getEnvelope(1), shortAmp);
  assert.deepEqual(shapes, [2]);
  assert.equal(backend.presets.status().modified, true);
});

test('the preview follows the values and envelopes', async () => {
  const backend = setUp();
  const kick = await backend.preview(29, 8);
  assert.ok(Math.abs(kick.tailHz - 43.65) < 0.01);

  backend.setEnvelope(1, shortAmp);
  backend.setValue('tune', 12);
  const shorter = await backend.preview(29, 8);

  assert.ok(shorter.samples.length < kick.samples.length / 2);
  assert.ok(Math.abs(shorter.tailHz - 87.31) < 0.01);
});

test('each instrument keeps the last note it played, starting from its default', () => {
  const backend = setUp();
  const { port, receive } = fakePort();
  backend.attach(port);
  const hits: number[] = [];
  backend.onHit((note) => hits.push(note));

  assert.equal(backend.lastNote(), 29);
  receive({ type: 'hit', note: 33 });
  assert.equal(backend.lastNote(), 33);

  backend.setValue('instrument', 1);
  assert.equal(backend.lastNote(), 55);

  backend.setValue('instrument', 0);
  assert.equal(backend.lastNote(), 33);
  assert.deepEqual(hits, [33]);
});

test('the worklet level reaches the meter as one channel', () => {
  const backend = setUp();
  const { port, receive } = fakePort();
  backend.attach(port);
  const levels: number[][] = [];
  backend.onLevel(({ peaks }) => levels.push(peaks));

  receive({ type: 'level', peak: 0.5 });
  assert.deepEqual(levels, [[0.5]]);
});
