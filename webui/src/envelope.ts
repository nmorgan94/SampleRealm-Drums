// Mirrors Source/dsp/BreakpointEnvelope.h and the editing rules of Source/envelopes/EnvelopeModel.cpp
import type { EnvelopeInfo, EnvelopeNode } from './backend.ts';

// EnvelopeModel::minGap and EnvelopeData::maxNodes
export const MIN_GAP = 0.0005;
export const MAX_NODES = 32;

const clamp = (value: number, min: number, max: number) => Math.min(max, Math.max(min, value));

//==============================================================================
// The value axis is linear in the domain: log2 Hz for a logarithmic envelope
export const toDomain = (info: EnvelopeInfo, value: number) => (info.scale === 'logarithmic' ? Math.log2(value) : value);
export const toDisplay = (info: EnvelopeInfo, value: number) => (info.scale === 'logarithmic' ? 2 ** value : value);
export const clampValue = (info: EnvelopeInfo, value: number) => clamp(value, info.min, info.max);

// Positive curve moves fast early, like an exponential decay; 0 is linear
export const applyCurve = (x: number, curve: number) => {
  const k = curve * 10;
  if (Math.abs(k) < 1e-3) return x;
  return (1 - Math.exp(-k * x)) / (1 - Math.exp(-k));
};

const interpolateSegment = (a: EnvelopeNode, b: EnvelopeNode, t: number) => {
  const span = b.time - a.time;
  if (span <= 0) return b.value;
  return a.value + (b.value - a.value) * applyCurve(clamp((t - a.time) / span, 0, 1), b.curve);
};

// Nodes in the domain, as EnvelopeModel::getData gives them
export const toDomainNodes = (info: EnvelopeInfo, nodes: readonly EnvelopeNode[]) =>
  nodes.map(({ time, value, curve }) => ({ time, value: toDomain(info, clampValue(info, value)), curve }));

export const evaluate = (nodes: readonly EnvelopeNode[], t: number) => {
  if (nodes.length === 0) return 0;
  if (t <= nodes[0]!.time) return nodes[0]!.value;

  for (let i = 1; i < nodes.length; i++) if (t < nodes[i]!.time) return interpolateSegment(nodes[i - 1]!, nodes[i]!, t);

  return nodes.at(-1)!.value;
};

export const duration = (nodes: readonly EnvelopeNode[]) => nodes.at(-1)?.time ?? 0;

//==============================================================================
const clampNode = (info: EnvelopeInfo, node: EnvelopeNode, minTime: number, maxTime: number): EnvelopeNode => ({
  time: clamp(node.time, minTime, Math.max(minTime, maxTime)),
  value: clampValue(info, node.value),
  curve: clamp(node.curve, -1, 1),
});

// EnvelopeModel::sanitiseEnv: time-ordered, the first at 0, the rest a gap apart; null if fewer than two
export function sanitise(info: EnvelopeInfo, nodes: readonly EnvelopeNode[]): EnvelopeNode[] | null {
  if (nodes.length < 2) return null;

  const sorted = [...nodes].sort((a, b) => a.time - b.time).slice(0, MAX_NODES);
  const result = [clampNode(info, sorted[0]!, 0, 0)];

  for (const node of sorted.slice(1)) result.push(clampNode(info, node, result.at(-1)!.time + MIN_GAP, info.maxTime));

  return result;
}

// The first node stays at t = 0; the others stay between their neighbours
export function setNode(info: EnvelopeInfo, nodes: readonly EnvelopeNode[], index: number, node: EnvelopeNode): EnvelopeNode[] {
  if (index < 0 || index >= nodes.length) return [...nodes];

  const isFirst = index === 0;
  const isLast = index === nodes.length - 1;
  const minTime = isFirst ? 0 : nodes[index - 1]!.time + MIN_GAP;
  const maxTime = isFirst ? 0 : isLast ? info.maxTime : nodes[index + 1]!.time - MIN_GAP;

  return nodes.map((existing, i) => (i === index ? clampNode(info, node, minTime, maxTime) : existing));
}

// New nodes go between two existing ones, never before the first or after the last
export function insertNode(info: EnvelopeInfo, nodes: readonly EnvelopeNode[], node: EnvelopeNode): { nodes: EnvelopeNode[]; index: number } | null {
  if (nodes.length >= MAX_NODES) return null;

  let index = 1;
  while (index < nodes.length && nodes[index]!.time < node.time) index++;
  if (index >= nodes.length) return null;

  const previous = nodes[index - 1]!.time;
  const next = nodes[index]!.time;
  if (next - previous < 2 * MIN_GAP) return null;

  const inserted = clampNode(info, node, previous + MIN_GAP, next - MIN_GAP);
  return { nodes: [...nodes.slice(0, index), inserted, ...nodes.slice(index)], index };
}

// The first and last nodes can't be removed
export const removeNode = (nodes: readonly EnvelopeNode[], index: number) =>
  index <= 0 || index >= nodes.length - 1 ? null : nodes.filter((_, i) => i !== index);

//==============================================================================
// EnvelopeEditor::updateView: spans the longest curve shown, after the time scale, with room to spare
export function viewSeconds(longest: number, timeScale: number) {
  const end = longest * timeScale * 1.15;
  for (const seconds of [0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6]) if (seconds >= end) return seconds;
  return Math.ceil(end);
}

// The first step that gives no more than maxLines lines across the span
export function gridStep(span: number, maxLines: number) {
  for (const step of [0.001, 0.002, 0.005, 0.01, 0.02, 0.025, 0.05, 0.1, 0.2, 0.25, 0.5, 1]) if (span / step <= maxLines) return step;
  return 2;
}

export function formatTime(seconds: number) {
  const ms = seconds * 1000;
  if (ms <= 0) return '0';
  if (ms >= 1000) return `${seconds.toFixed(2)} s`;
  if (ms >= 10) return `${Math.round(ms)} ms`;
  return `${ms.toFixed(1)} ms`;
}

// Value-axis lines: 1, 2 and 5 per decade for Hz, quarters for gain
export function gridValues(info: EnvelopeInfo) {
  if (info.scale === 'linear') return [0, 0.25, 0.5, 0.75, 1].map((p) => info.min + (info.max - info.min) * p);

  const values: number[] = [];
  for (let decade = 10 ** Math.floor(Math.log10(info.min)); decade <= info.max; decade *= 10) {
    for (const multiple of [1, 2, 5]) {
      const value = decade * multiple;
      if (value >= info.min && value <= info.max) values.push(value);
    }
  }
  return values;
}
