// The C++ side of these names is Source/webview/WebViewEditor.cpp
import { getNativeFunction, getSliderState } from '@juce-framework/webview';
import { paramLookup, type DrumsBackend, type EnvelopeNode, type Level, type PresetBackend, type PresetEntry, type PresetStatus, type UiInfo } from '../backend.ts';
import { toProportion } from '../params.ts';

// JUCE's type lists only its own initialisation data
type InitialisationData = { uiInfo: string[]; presetStatus: string[]; envelopes: string[]; lastNotes: string[] };

const call = (name: string, ...args: unknown[]) => getNativeFunction(name)(...args);

const listen = (event: string, listener: (payload: unknown) => void) => {
  const handle = window.__JUCE__.backend.addEventListener(event, listener);
  return () => window.__JUCE__.backend.removeEventListener(handle);
};

function jucePresets(initial: PresetStatus): PresetBackend {
  let status = initial;
  const listeners = new Set<(status: PresetStatus) => void>();

  const update = (payload: unknown) => {
    status = payload as PresetStatus;
    listeners.forEach((listener) => listener(status));
  };

  // Asked again once listening, in case a change came first
  listen('presetStatus', update);
  call('presetStatus').then(update);
  const error = async (result: Promise<unknown>) => ((await result) as string) || null;

  return {
    status: () => status,
    onStatus: (listener) => {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    list: async () => (await call('presetList')) as PresetEntry[],
    load: async (preset) => void (await call('presetLoad', preset)),
    loadInit: async () => void (await call('presetInit')),
    loadNext: async () => void (await call('presetStep', 1)),
    loadPrevious: async () => void (await call('presetStep', -1)),
    exists: async (name) => (await call('presetExists', name)) as boolean,
    save: (name, category) => error(call('presetSave', name, category)),
    rename: (preset, newName) => error(call('presetRename', preset, newName)),
    remove: (preset) => error(call('presetDelete', preset)),
    showFolder: () => void call('presetShowFolder'),
  };
}

const decodeSamples = (base64: string) => {
  const bytes = Uint8Array.from(atob(base64), (c) => c.charCodeAt(0));
  return new Float32Array(bytes.buffer, 0, Math.floor(bytes.length / 4));
};

export function juceBackend(): DrumsBackend {
  const data = window.__JUCE__.initialisationData as unknown as InitialisationData;
  const info = JSON.parse(data.uiInfo[0]!) as UiInfo;
  const entry = paramLookup(new Map(info.params.map((param) => [param.id, { param, slider: getSliderState(param.id) }])));

  const param = (id: string) => entry(id).param;
  const slider = (id: string) => entry(id).slider;

  let envelopes = JSON.parse(data.envelopes[0]!) as EnvelopeNode[][];
  let lastNotes = JSON.parse(data.lastNotes[0]!) as number[];
  const envelopeListeners = info.envelopes.map(() => new Set<(nodes: EnvelopeNode[]) => void>());

  const notifyEnvelope = (index: number) => envelopeListeners[index]?.forEach((listener) => listener(envelopes[index]!));

  // Presets and state restores
  listen('envelopes', (payload) => {
    envelopes = payload as EnvelopeNode[][];
    envelopes.forEach((_, index) => notifyEnvelope(index));
  });

  listen('lastNotes', (payload) => (lastNotes = payload as number[]));

  return {
    version: info.version,
    presets: jucePresets(JSON.parse(data.presetStatus[0]!) as PresetStatus),
    param,
    getValue: (id) => slider(id).getScaledValue(),
    setValue: (id, value) => slider(id).setNormalisedValue(toProportion(param(id), value)),
    beginGesture: (id) => slider(id).sliderDragStarted(),
    endGesture: (id) => slider(id).sliderDragEnded(),
    subscribe: (id, listener) => {
      const state = slider(id);
      const handle = state.valueChangedEvent.addListener(() => listener(state.getScaledValue()));
      return () => state.valueChangedEvent.removeListener(handle);
    },
    onLevel: (listener) => listen('level', (payload) => listener(payload as Level)),
    envelopes: info.envelopes,
    getEnvelope: (index) => envelopes[index]!,
    setEnvelope: (index, nodes) => {
      envelopes = envelopes.map((existing, i) => (i === index ? nodes : existing));
      notifyEnvelope(index);
      void call('setEnvelope', index, nodes);
    },
    subscribeEnvelope: (index, listener) => {
      envelopeListeners[index]?.add(listener);
      return () => envelopeListeners[index]?.delete(listener);
    },
    audition: () => void call('audition'),
    onHit: (listener) => listen('hit', (note) => listener(note as number)),
    lastNote: () => lastNotes[Math.round(slider('instrument').getScaledValue())] ?? 0,
    preview: async (note, maxSeconds) => {
      const result = (await call('preview', note, maxSeconds)) as { samples: string; sampleRate: number; tailHz: number };
      return { samples: decodeSamples(result.samples), sampleRate: result.sampleRate, tailHz: result.tailHz };
    },
  };
}
