import type { Engine } from '@samplerealm/modules/demo/engine.ts';
import { browserStorage, xmlTags, type PresetStorage } from '@samplerealm/modules/demo/localPresets.ts';
import { workletBackend, type WorkletBackend } from '@samplerealm/modules/demo/workletBackend.ts';
import { listenerSet, type DrumsBackend, type EnvelopeNode, type Loop, type LoopSteps, type UiInfo } from '../backend.ts';
import { sanitise } from '../envelope.ts';
import { drumText } from '../params.ts';
import { flattenNodes, readDrumInfo, readLoopSteps, renderPreview, setEnvelope } from './engine.ts';

// This plugin's, as a site's demos share its storage. Unchanged, so saved presets stay
export const STORAGE_KEY = 'samplerealm-drums-presets';

// As PresetManager::Config::typeParameterId
const TYPE_PARAMETER = 'instrument';

// The exports processor.js reports when one changes
export const WATCHED = ['hitCount', 'lastNote', 'lastHitLevel', 'lastHitVelocity', 'playingStep'];

// By the envelope's id, as a preset holds them
type Envelopes = Record<string, EnvelopeNode[]>;

export type DemoBackend = DrumsBackend & WorkletBackend;

// The ENVELOPES tree PresetManager writes
export const parseEnvelopes = (xml: string): Envelopes =>
  Object.fromEntries(
    [...xml.matchAll(/<ENV\s[^>]*>([\s\S]*?)<\/ENV>/g)].map(([env, body]) => [
      xmlTags(env, 'ENV')[0]?.id ?? '',
      xmlTags(body!, 'NODE').map(({ t, v, c }) => ({ time: Number(t), value: Number(v), curve: Number(c) })),
    ])
  );

// Presets saved before the envelopes moved into `extra` held them beside the values
const moveOldEnvelopes = (storage: PresetStorage) => {
  type Stored = { envelopes?: Envelopes; extra?: unknown } | null;
  const isOld = (preset: Stored) => preset?.envelopes !== undefined && !preset.extra;

  try {
    const stored: unknown = JSON.parse(storage.getItem(STORAGE_KEY) ?? '');
    if (!Array.isArray(stored) || !stored.some(isOld)) return;

    storage.setItem(
      STORAGE_KEY,
      JSON.stringify(
        stored.map((preset: Stored) => {
          if (!preset || !isOld(preset)) return preset;
          const { envelopes, ...rest } = preset;
          return { ...rest, extra: { envelopes } };
        })
      )
    );
  } catch {
    // Nothing stored, or not presets
  }
};

interface Options {
  // The factory preset files' text
  factoryPresets?: string[];
  // The page's own unless given
  storage?: PresetStorage;
}

// The worklet's engine plays; previewEngine, on the page, renders the waveform
export function demoBackend(info: UiInfo, previewEngine: Engine, { factoryPresets = [], storage = browserStorage() }: Options = {}): DemoBackend {
  const drums = readDrumInfo(previewEngine);
  const envelopes = drums.envelopes.map((envelope) => envelope.nodes);
  const envelopeListeners = drums.envelopes.map(() => listenerSet<[EnvelopeNode[]]>());
  const hitListeners = listenerSet<[number, number, number]>();
  // Each instrument keeps its own last note once it has played
  const lastNotes = new Map<number, number>();
  let hitCount = previewEngine.hitCount();
  let loop: Loop = { pattern: 0, on: false };
  let playing = -1;
  const loopStepListeners = listenerSet<[LoopSteps]>();

  previewEngine.prepare(previewEngine.previewSampleRate(), 1);
  if (storage) moveOldEnvelopes(storage);

  const backend = workletBackend<{ envelopes: Envelopes }>(info, {
    storageKey: STORAGE_KEY,
    factoryPresets,
    storage,
    typeParameter: TYPE_PARAMETER,
    extraState: {
      parse: (xml) => ({ envelopes: parseEnvelopes(xml) }),
      capture: () => ({ envelopes: Object.fromEntries(drums.envelopes.map((envelope, index) => [envelope.id, envelopes[index]!])) }),
      // Invalid or missing envelopes take their defaults
      apply: ({ envelopes: stored = {} }) =>
        drums.envelopes.forEach((envelope, index) => applyEnvelope(index, sanitise(envelope, stored[envelope.id] ?? []) ?? envelope.nodes)),
    },
  });

  const applyEnvelope = (index: number, nodes: EnvelopeNode[]) => {
    const flat = flattenNodes(nodes);
    envelopes[index] = nodes;
    backend.post({ type: 'call', name: 'setEnvelope', args: [index, flat.length / 3], floats: { at: 'envelopeNodes', values: flat } });
    setEnvelope(previewEngine, index, nodes);
    envelopeListeners[index]?.notify(nodes);
  };

  const instrument = () => Math.round(backend.getValue(TYPE_PARAMETER));

  info.params.forEach(({ id }, index) => backend.subscribe(id, (value) => previewEngine.setParam(index, value)));

  // The page's engine holds the steps as the worklet's does; only the worklet's plays them
  let steps = readLoopSteps(previewEngine);
  const loopSteps = (): LoopSteps => ({ steps, playing });
  const notifyLoopSteps = () => loopStepListeners.notify(loopSteps());

  const reloadLoopSteps = () => {
    steps = readLoopSteps(previewEngine);
    notifyLoopSteps();
  };

  backend.subscribe(TYPE_PARAMETER, reloadLoopSteps);

  // lastNote changes with the instrument too: only a new hit count is a hit
  backend.onMessage((data) => {
    if (data.type !== 'watch') return;
    const watched = data.values as { hitCount: number; lastNote: number; lastHitLevel: number; lastHitVelocity: number; playingStep?: number };

    if (watched.playingStep !== undefined && watched.playingStep !== playing) {
      playing = watched.playingStep;
      notifyLoopSteps();
    }

    if (watched.hitCount === hitCount) return;

    hitCount = watched.hitCount;
    lastNotes.set(instrument(), watched.lastNote);
    hitListeners.notify(watched.lastNote, watched.lastHitLevel, watched.lastHitVelocity);
  });

  return {
    ...backend,
    ...drumText(info, drums),
    envelopes: drums.envelopes,
    getEnvelope: (index) => envelopes[index]!,
    setEnvelope: (index, nodes) => {
      applyEnvelope(index, nodes);
      backend.markModified();
    },
    subscribeEnvelope: (index, listener) => envelopeListeners[index]?.add(listener) ?? (() => {}),
    audition: (velocity) => backend.post({ type: 'call', name: 'audition', args: [velocity] }),
    patterns: drums.patterns,
    getLoop: () => loop,
    setLoop: (next) => {
      loop = next;
      backend.post({ type: 'call', name: 'setLoop', args: [next.pattern, next.on ? 1 : 0] });
      previewEngine.setLoop(next.pattern, 0);
      reloadLoopSteps();
    },
    getLoopSteps: loopSteps,
    setLoopStep: (step, velocity) => {
      backend.post({ type: 'call', name: 'setLoopStep', args: [step, velocity] });
      previewEngine.setLoopStep(step, velocity);
      reloadLoopSteps();
    },
    subscribeLoopSteps: loopStepListeners.add,
    onHit: hitListeners.add,
    // Until an instrument has played, the engine's default for it
    lastNote: () => lastNotes.get(instrument()) ?? previewEngine.lastNote(),
    preview: async (note, maxSeconds) => renderPreview(previewEngine, note, maxSeconds),
  };
}
