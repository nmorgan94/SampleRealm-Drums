// The C++ side of these names is Source/webview/DrumsWebView.cpp
import { getNativeFunction } from '@juce-framework/webview';
import { initialData, onEvent, pluginBackend } from '@samplerealm/modules/plugin/pluginBackend.ts';
import { listenerSet, type DrumInfo, type DrumsBackend, type EnvelopeNode } from '../backend.ts';
import { drumText } from '../params.ts';

const call = (name: string, ...args: unknown[]) => getNativeFunction(name)(...args);

const decodeSamples = (base64: string) => {
  const bytes = Uint8Array.from(atob(base64), (c) => c.charCodeAt(0));
  return new Float32Array(bytes.buffer, 0, Math.floor(bytes.length / 4));
};

// The shared plugin backend, with the envelopes, the hits and the preview
export function juceBackend(): DrumsBackend {
  const backend = pluginBackend();
  const drums = JSON.parse(initialData<string>('drumInfo')) as DrumInfo;

  let envelopes = JSON.parse(initialData<string>('envelopes')) as EnvelopeNode[][];
  let lastNotes = JSON.parse(initialData<string>('lastNotes')) as number[];
  const envelopeListeners = drums.envelopes.map(() => listenerSet<[EnvelopeNode[]]>());

  const notifyEnvelope = (index: number) => envelopeListeners[index]?.notify(envelopes[index]!);

  // Presets and state restores
  onEvent<EnvelopeNode[][]>('envelopes', (payload) => {
    envelopes = payload;
    envelopes.forEach((_, index) => notifyEnvelope(index));
  });

  onEvent<number[]>('lastNotes', (payload) => (lastNotes = payload));

  return {
    ...backend,
    ...drumText(backend.info, drums),
    envelopes: drums.envelopes,
    getEnvelope: (index) => envelopes[index]!,
    setEnvelope: (index, nodes) => {
      envelopes = envelopes.map((existing, i) => (i === index ? nodes : existing));
      notifyEnvelope(index);
      void call('setEnvelope', index, nodes);
    },
    subscribeEnvelope: (index, listener) => envelopeListeners[index]?.add(listener) ?? (() => {}),
    audition: () => void call('audition'),
    onHit: (listener) => onEvent<number>('hit', listener),
    lastNote: () => lastNotes[Math.round(backend.getValue('instrument'))] ?? 0,
    preview: async (note, maxSeconds) => {
      const result = (await call('preview', note, maxSeconds)) as { samples: string; sampleRate: number; tailHz: number };
      return { samples: decodeSamples(result.samples), sampleRate: result.sampleRate, tailHz: result.tailHz };
    },
  };
}
