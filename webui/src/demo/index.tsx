import { useCallback, useEffect, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { DemoFrame } from '@samplerealm/modules/demo/DemoFrame.tsx';
import { instantiateEngine, loadEngine, startWorklet } from '@samplerealm/modules/demo/engine.ts';
import processorUrl from '@samplerealm/modules/demo/processor.js?url';
import { qwertySemitone } from '@samplerealm/modules/demo/qwerty.ts';
import { juceHeight } from '@samplerealm/modules/editor/fonts.ts';
import { EDITOR_HEIGHT, EDITOR_WIDTH, Editor } from '../Editor.tsx';
import { clamp } from '../envelope.ts';
import { demoBackend, WATCHED, type DemoBackend } from './demoBackend.ts';
import { factoryPresets } from './factoryPresets.ts';
import { Keyboard } from './Keyboard.tsx';
import './styles.css';

const ENGINE_URL = 'engine.wasm';

// C1 up two octaves: Key Track tunes kicks around here
const LOW_NOTE = 24;
const NUM_KEYS = 25;

// Left and right: the cymbal's Width spreads them
const CHANNELS = 2;

// Transport::bpm, until the page says otherwise
const DEFAULT_BPM = 174;
const MIN_BPM = 40;
const MAX_BPM = 300;

const isTyping = (target: EventTarget | null) => target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement;

async function startAudio(backend: DemoBackend, wasm: ArrayBuffer, context: AudioContext) {
  const node = await startWorklet(context, processorUrl, {
    numberOfInputs: 0,
    outputChannelCount: [2],
    processorOptions: { wasm, channels: CHANNELS, watch: WATCHED },
  });

  node.connect(context.destination);
  backend.attach(node.port);
}

// A second engine on the page renders the preview
async function loadDemo() {
  const { wasm, info, module } = await loadEngine(ENGINE_URL);
  return { backend: demoBackend(info, instantiateEngine(module), { factoryPresets }), wasm };
}

function Demo() {
  const [engine, setEngine] = useState<{ backend: DemoBackend; wasm: ArrayBuffer } | null>(null);
  const [status, setStatus] = useState<'loading' | 'ready' | 'failed'>('loading');
  const [active, setActive] = useState<ReadonlySet<number>>(new Set());
  const [midi, setMidi] = useState<MIDIAccess | null>(null);
  const [bpm, setBpm] = useState(DEFAULT_BPM);
  const audio = useRef<AudioContext | null>(null);
  const starting = useRef<Promise<void> | null>(null);

  // The loop's tempo; a half-typed one isn't sent, and leaving the box brings it into range
  useEffect(() => {
    if (bpm >= MIN_BPM && bpm <= MAX_BPM) engine?.backend.setTransport({ bpm, playing: false, ppq: 0 });
  }, [engine, bpm]);

  useEffect(() => {
    loadDemo()
      .then(setEngine)
      .catch((error) => {
        console.error('Demo engine failed to load:', error);
        setStatus('failed');
      });
  }, []);

  // Browsers only start audio from a click or key press
  const start = useCallback(() => {
    if (!engine) return Promise.resolve();
    if (starting.current) {
      // Wakes audio a browser held back when it started without a click
      audio.current?.resume();
      return starting.current;
    }

    const context = new AudioContext();
    context.resume();
    audio.current = context;

    starting.current = startAudio(engine.backend, engine.wasm, context)
      .then(() => setStatus('ready'))
      .catch((error) => {
        console.error('Demo audio failed to start:', error);
        context.close();
        audio.current = null;
        starting.current = null;
        setStatus('failed');
      });

    return starting.current;
  }, [engine]);

  // The site opens the demo from a click, which allow="autoplay" passes on
  useEffect(() => {
    start();
  }, [start]);

  // Any click in the demo (the Hit pad included) wakes audio a browser held back
  const wake = useCallback(() => void start(), [start]);

  const noteOn = useCallback(
    (note: number) => {
      setActive((current) => new Set(current).add(note));
      start();
      engine?.backend.noteOn(note);
    },
    [engine, start]
  );

  // A drum ignores note-off; the key just lets go
  const noteOff = useCallback((note: number) => {
    setActive((current) => {
      const next = new Set(current);
      next.delete(note);
      return next;
    });
  }, []);

  useEffect(() => {
    const held = new Map<string, number>();

    const down = (e: KeyboardEvent) => {
      const semitone = qwertySemitone(e.code);
      if (semitone === undefined || e.metaKey || e.ctrlKey || e.altKey || isTyping(e.target)) return;
      // ' opens Firefox's quick find
      e.preventDefault();
      if (e.repeat || held.has(e.code)) return;
      held.set(e.code, LOW_NOTE + semitone);
      noteOn(LOW_NOTE + semitone);
    };

    const up = (e: KeyboardEvent) => {
      const note = held.get(e.code);
      if (note === undefined) return;
      held.delete(e.code);
      noteOff(note);
    };

    // No keyup arrives once the frame loses focus
    const releaseAll = () => {
      held.forEach((note) => noteOff(note));
      held.clear();
    };

    window.addEventListener('keydown', down);
    window.addEventListener('keyup', up);
    window.addEventListener('blur', releaseAll);
    return () => {
      window.removeEventListener('keydown', down);
      window.removeEventListener('keyup', up);
      window.removeEventListener('blur', releaseAll);
      releaseAll();
    };
  }, [noteOn, noteOff]);

  const connectMidi = async () => {
    if (!engine) return;
    try {
      await start();
      const access = await navigator.requestMIDIAccess();
      const listen = (input: MIDIInput) => (input.onmidimessage = (e) => e.data && engine.backend.midi(e.data));
      access.inputs.forEach(listen);
      access.onstatechange = (e) => {
        if (e.port instanceof MIDIInput) listen(e.port);
      };
      setMidi(access);
    } catch (error) {
      console.error('MIDI access refused:', error);
    }
  };

  useEffect(
    () => () => {
      if (!midi) return;
      midi.onstatechange = null;
      midi.inputs.forEach((input) => (input.onmidimessage = null));
    },
    [midi]
  );

  useEffect(() => () => void audio.current?.close(), []);

  return (
    <div onPointerDown={wake}>
      <DemoFrame width={EDITOR_WIDTH} height={EDITOR_HEIGHT} status={status} editor={engine && <Editor backend={engine.backend} />}>
        <Keyboard lowNote={LOW_NOTE} numKeys={NUM_KEYS} active={active} onNoteOn={noteOn} onNoteOff={noteOff} />

        <label className="mt-2.5 mr-2.5 inline-flex h-7 items-center gap-2 rounded border border-line bg-widget pl-3 text-fg" style={{ fontSize: juceHeight(13) }}>
          Tempo
          <input
            type="number"
            className="h-full w-16 rounded-r bg-background px-2 text-fg"
            min={MIN_BPM}
            max={MAX_BPM}
            value={Number.isNaN(bpm) ? '' : bpm}
            onChange={(e) => setBpm(e.target.valueAsNumber)}
            onBlur={() => setBpm(Number.isNaN(bpm) ? DEFAULT_BPM : clamp(bpm, MIN_BPM, MAX_BPM))}
          />
        </label>

        {status === 'ready' && !midi && 'requestMIDIAccess' in navigator && (
          <button
            type="button"
            className="mt-2.5 h-7 cursor-pointer rounded border border-line bg-widget px-3 text-fg"
            style={{ fontSize: juceHeight(13) }}
            onClick={connectMidi}
          >
            Use a MIDI keyboard
          </button>
        )}
      </DemoFrame>
    </div>
  );
}

const root = document.getElementById('root');
if (root) createRoot(root).render(<Demo />);
