import assert from 'node:assert/strict';
import { blockSize, engineAt, sampleRate } from '@samplerealm/modules/testing/engine.ts';
import { localEnginePath } from '../localEngine.ts';
import { paramLookup, type EnvelopeNode } from '../src/backend.ts';
import { readDrumInfo, setEnvelope } from '../src/demo/engine.ts';

const monoEngine = engineAt(localEnginePath());

// Makes a fresh engine from the last wasm build, prepared for its one channel, or unprepared for null
export function engine(rate?: number | null) {
  const made = monoEngine(1, rate);

  const envelope = (id: string, nodes: readonly EnvelopeNode[]) => {
    const index = drums.envelopes.findIndex((spec) => spec.id === id);
    assert.ok(index >= 0, `no envelope ${id}`);
    setEnvelope(made.engine, index, nodes);
  };

  const block = (length = blockSize) => made.renderBlocks(length)[0]!;

  const play = (seconds: number) => made.renderBlocks(Math.round((seconds * sampleRate) / blockSize) * blockSize)[0]!;

  return { ...made, envelope, block, play };
}

const first = engine();

export const { info } = first;

export const drums = readDrumInfo(first.engine);

export const param = paramLookup(new Map(info.params.map((p) => [p.id, p])));
