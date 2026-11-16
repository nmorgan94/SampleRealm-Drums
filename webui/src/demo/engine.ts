// What Drums' engine exports beside the shared ones: wasm/Engine.cpp
import { readJson, type Engine } from '@samplerealm/modules/demo/engine.ts';
import type { DrumInfo, EnvelopeNode, Preview } from '../backend.ts';
import { MAX_NODES } from '../envelope.ts';

export const readDrumInfo = (engine: Engine) => readJson<DrumInfo>(engine, engine.drumInfo());

// { time, value, curve } per node; no more than envelopeNodes() holds
export const flattenNodes = (nodes: readonly EnvelopeNode[]) =>
  nodes.slice(0, MAX_NODES).flatMap(({ time, value, curve }) => [time, value, curve]);

export const setEnvelope = (engine: Engine, index: number, nodes: readonly EnvelopeNode[]) => {
  const flat = flattenNodes(nodes);
  new Float32Array(engine.memory.buffer, engine.envelopeNodes(), flat.length).set(flat);
  engine.setEnvelope(index, flat.length / 3);
};

export const renderPreview = (engine: Engine, note: number, maxSeconds: number): Preview => {
  const pointer = engine.preview(note, maxSeconds);
  return {
    samples: new Float32Array(engine.memory.buffer, pointer, engine.previewLength()).slice(),
    sampleRate: engine.previewSampleRate(),
    tailHz: engine.previewTailHz(),
  };
};
