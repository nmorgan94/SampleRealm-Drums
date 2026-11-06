import assert from 'node:assert/strict';
import { test } from 'node:test';
import {
  applyCurve, evaluate, formatTime, gridStep, gridValues, insertNode, MAX_NODES, MIN_GAP, removeNode, sanitise, setNode, toDomainNodes, viewSeconds,
} from '../src/envelope.ts';
import { info } from './engine.ts';

const pitch = info.envelopes.find((e) => e.id === 'pitch')!;
const amp = info.envelopes.find((e) => e.id === 'amp')!;

test('curves bend each segment, 0 staying linear', () => {
  assert.equal(applyCurve(0.5, 0), 0.5);
  assert.ok(applyCurve(0.5, 0.5) > 0.5);
  assert.ok(applyCurve(0.5, -0.5) < 0.5);
  assert.ok(Math.abs(applyCurve(1, 0.8) - 1) < 1e-12);
});

test('pitch envelopes run in log2 Hz, and hold their last value', () => {
  const nodes = toDomainNodes(pitch, [{ time: 0, value: 400, curve: 0 }, { time: 1, value: 100, curve: 0 }]);
  assert.ok(Math.abs(2 ** evaluate(nodes, 0.5) - 200) < 1e-9);
  assert.ok(Math.abs(2 ** evaluate(nodes, 5) - 100) < 1e-9);
  assert.ok(Math.abs(2 ** toDomainNodes(pitch, [{ time: 0, value: 5, curve: 0 }])[0]!.value - pitch.min) < 1e-9);
});

test('a dragged node stays between its neighbours and in range, the first at t = 0', () => {
  const nodes = amp.nodes;

  assert.deepEqual(setNode(amp, nodes, 0, { time: 0.5, value: 2, curve: 3 })[0], { time: 0, value: 1, curve: 1 });
  assert.equal(setNode(amp, nodes, 1, { time: 1, value: 0.5, curve: 0 })[1]!.time, nodes[2]!.time - MIN_GAP);
  assert.equal(setNode(amp, nodes, 3, { time: 9, value: -1, curve: 0 }).at(-1)!.time, amp.maxTime);
  assert.equal(setNode(amp, nodes, 3, { time: 9, value: -1, curve: 0 }).at(-1)!.value, 0);
});

test('nodes are inserted between existing ones only, and the ends cannot be removed', () => {
  const inserted = insertNode(amp, amp.nodes, { time: 0.1, value: 0.9, curve: 0 });
  assert.equal(inserted?.index, 2);
  assert.equal(inserted?.nodes.length, amp.nodes.length + 1);

  assert.equal(insertNode(amp, amp.nodes, { time: 5, value: 0, curve: 0 }), null);
  assert.equal(removeNode(amp.nodes, 0), null);
  assert.equal(removeNode(amp.nodes, amp.nodes.length - 1), null);
  assert.equal(removeNode(amp.nodes, 1)?.length, amp.nodes.length - 1);

  const full = Array.from({ length: MAX_NODES }, (_, i) => ({ time: i * 0.01, value: 0.5, curve: 0 }));
  assert.equal(insertNode(amp, full, { time: 0.005, value: 0.5, curve: 0 }), null);
});

test('stored envelopes are sorted, spaced and clamped, or rejected under two nodes', () => {
  assert.deepEqual(
    sanitise(amp, [{ time: 0.2, value: 3, curve: 0 }, { time: 0.1, value: 0.5, curve: 0 }, { time: 0.1, value: 0.5, curve: 0 }]),
    [{ time: 0, value: 0.5, curve: 0 }, { time: 0.1, value: 0.5, curve: 0 }, { time: 0.2, value: 1, curve: 0 }]
  );
  assert.equal(sanitise(amp, [{ time: 0, value: 1, curve: 0 }]), null);
});

test('the view spans the longest curve with room to spare, after Length', () => {
  assert.equal(viewSeconds(0.35, 1), 0.5);
  assert.equal(viewSeconds(0.35, 2), 1);
  assert.equal(viewSeconds(4, 4), 19);
});

test('grid lines and times read as the plugin draws them', () => {
  assert.equal(gridStep(0.5, 10), 0.05);
  assert.deepEqual(gridValues(amp), [0, 0.25, 0.5, 0.75, 1]);
  assert.deepEqual(gridValues(pitch), [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000]);
  assert.deepEqual([0, 0.0042, 0.35, 1.5].map(formatTime), ['0', '4.2 ms', '350 ms', '1.50 s']);
});
