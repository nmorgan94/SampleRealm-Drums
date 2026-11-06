import { paramLookup, type DrumsBackend, type EnvelopeNode, type Level, type UiInfo } from '../backend.ts';
import { sanitise } from '../envelope.ts';
import { flattenNodes, renderPreview, setEnvelope, type Engine } from './engine.ts';
import { localPresets, type Envelopes, type PresetStorage, type StoredPreset } from './localPresets.ts';

// The node's port, so tests can stand in for it
export type EnginePort = Pick<MessagePort, 'postMessage'> & { onmessage: ((e: MessageEvent) => void) | null };

const NOTE_ON = 0x90;
const VELOCITY = 100;

// As PresetManager::Config::typeParameterId
const TYPE_PARAMETER = 'instrument';

export type WorkletBackend = DrumsBackend & {
  attach(port: EnginePort): void;
  midi(bytes: ArrayLike<number>): void;
  noteOn(note: number): void;
};

// The worklet's engine plays; previewEngine, on the page, renders the waveform
export function workletBackend(info: UiInfo, previewEngine: Engine, factoryPresets: StoredPreset[] = [], storage?: PresetStorage): WorkletBackend {
  const entry = paramLookup(new Map(info.params.map((param, index) => [param.id, { param, index, value: param.defaultValue }])));
  const listeners = new Map<string, Set<(value: number) => void>>();
  const envelopeListeners = info.envelopes.map(() => new Set<(nodes: EnvelopeNode[]) => void>());
  const levelListeners = new Set<(level: Level) => void>();
  const hitListeners = new Set<(lastNote: number) => void>();
  const envelopes = info.envelopes.map((envelope) => envelope.nodes);
  // Each instrument keeps its own last note once it has played
  const lastNotes = new Map<number, number>();
  let port: EnginePort | null = null;
  // Sent once the worklet attaches
  let pending: unknown[] = [];

  previewEngine.prepare(previewEngine.previewSampleRate());

  const post = (message: unknown) => {
    if (port) port.postMessage(message);
    else pending.push(message);
  };

  const apply = (id: string, value: number) => {
    const found = entry(id);
    post({ type: 'param', index: found.index, value });
    previewEngine.setParam(found.index, value);
    found.value = value;
    listeners.get(id)?.forEach((listener) => listener(value));
  };

  const applyEnvelope = (index: number, nodes: EnvelopeNode[]) => {
    envelopes[index] = nodes;
    post({ type: 'envelope', index, nodes: flattenNodes(nodes) });
    setEnvelope(previewEngine, index, nodes);
    envelopeListeners[index]?.forEach((listener) => listener(nodes));
  };

  const instrument = () => Math.round(entry(TYPE_PARAMETER).value);

  const presets = localPresets({
    factory: factoryPresets,
    storage,
    defaults: Object.fromEntries(info.params.map((param) => [param.id, param.defaultValue])),
    typeParameter: TYPE_PARAMETER,
    currentType: () => entry(TYPE_PARAMETER).param.choices[instrument()] ?? '',
    values: () => Object.fromEntries(info.params.map((param) => [param.id, entry(param.id).value])),
    envelopes: () => Object.fromEntries(info.envelopes.map((envelope, index) => [envelope.id, envelopes[index]!])),
    // Ids this engine doesn't have are skipped, as the APVTS skips them; invalid envelopes take their defaults
    apply: (values, stored: Envelopes) => {
      info.params.forEach(({ id }) => id in values && apply(id, values[id]!));
      info.envelopes.forEach((envelope, index) => applyEnvelope(index, sanitise(envelope, stored[envelope.id] ?? []) ?? envelope.nodes));
    },
  });

  const midi = (bytes: ArrayLike<number>) => {
    const [status = 0, data1 = 0, data2 = 0] = Array.from(bytes);
    post({ type: 'midi', bytes: [status, data1, data2] });
  };

  return {
    version: info.version,
    presets,
    envelopes: info.envelopes,
    param: (id) => entry(id).param,
    getValue: (id) => entry(id).value,
    setValue: (id, value) => {
      apply(id, value);
      presets.markModified();
    },
    beginGesture: () => {},
    endGesture: () => {},
    subscribe: (id, listener) => {
      const set = listeners.get(id) ?? new Set();
      listeners.set(id, set.add(listener));
      return () => set.delete(listener);
    },
    onLevel: (listener) => {
      levelListeners.add(listener);
      return () => levelListeners.delete(listener);
    },
    getEnvelope: (index) => envelopes[index]!,
    setEnvelope: (index, nodes) => {
      applyEnvelope(index, nodes);
      presets.markModified();
    },
    subscribeEnvelope: (index, listener) => {
      envelopeListeners[index]?.add(listener);
      return () => envelopeListeners[index]?.delete(listener);
    },
    audition: () => post({ type: 'audition' }),
    onHit: (listener) => {
      hitListeners.add(listener);
      return () => hitListeners.delete(listener);
    },
    // Until an instrument has played, the engine's default for it
    lastNote: () => lastNotes.get(instrument()) ?? previewEngine.lastNote(),
    preview: async (note, maxSeconds) => renderPreview(previewEngine, note, maxSeconds),
    midi,
    noteOn: (note) => midi([NOTE_ON, note, VELOCITY]),
    attach: (enginePort) => {
      port = enginePort;
      port.onmessage = ({ data }) => {
        if (data.type === 'level') levelListeners.forEach((listener) => listener({ peaks: [data.peak] }));
        else if (data.type === 'hit') {
          lastNotes.set(instrument(), data.note);
          hitListeners.forEach((listener) => listener(data.note));
        }
      };
      pending.forEach((message) => enginePort.postMessage(message));
      pending = [];
    },
  };
}
