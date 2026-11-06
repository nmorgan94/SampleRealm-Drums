import type { EnvelopeNode, Preview, UiInfo } from '../backend.ts';
import { MAX_NODES } from '../envelope.ts';

// WASI's clock only seeds juce::Random, which every hit reseeds
export const stubImports = () => {
  const stub = new Proxy({}, { get: () => () => 0 });
  return new Proxy({}, { get: () => stub }) as WebAssembly.Imports;
};

export type Engine = Record<string, CallableFunction> & { memory: WebAssembly.Memory };

export const readUiInfo = (engine: Engine) => {
  const bytes = new Uint8Array(engine.memory.buffer, engine.uiInfo());
  return JSON.parse(new TextDecoder().decode(bytes.subarray(0, bytes.indexOf(0)))) as UiInfo;
};

export const instantiate = (wasm: BufferSource) => {
  const engine = new WebAssembly.Instance(new WebAssembly.Module(wasm), stubImports()).exports as Engine;
  engine._initialize?.();
  return engine;
};

export async function loadEngine(url: string): Promise<{ wasm: ArrayBuffer; engine: Engine; info: UiInfo }> {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`${url}: ${response.status}`);

  const wasm = await response.arrayBuffer();
  const engine = instantiate(wasm);
  return { wasm, engine, info: readUiInfo(engine) };
}

// As processor.js does in the worklet; no more than envelopeNodes() holds
export const flattenNodes = (nodes: readonly EnvelopeNode[]) =>
  Float32Array.from(nodes.slice(0, MAX_NODES).flatMap(({ time, value, curve }) => [time, value, curve]));

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
