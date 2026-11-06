// Mirrors Source/PluginEditor.cpp, with the boxes resized() gives
import { memo, useCallback, useEffect, useState } from 'react';
import type { DrumsBackend, Preview } from './backend.ts';
import { ChoiceBox, ChoiceButtons, Knob, Toggle } from './controls.tsx';
import { EnvelopeEditor, type Curve } from './EnvelopeEditor.tsx';
import { useHits, useParam } from './hooks.ts';
import { layOutRow, type Box } from './layout.ts';
import { LevelMeter } from './LevelMeter.tsx';
import { minimumWidth, Panel, panelLayout } from './Panel.tsx';
import { hertzText } from './params.ts';
import { PresetBar } from './PresetBar.tsx';
import { usePrompt } from './PromptOverlay.tsx';
import { Text } from './Text.tsx';
import { TriggerPad } from './TriggerPad.tsx';

export const EDITOR_WIDTH = 1080;
export const EDITOR_HEIGHT = 610;

const MAX_PREVIEW_SECONDS = 8;

const BOXES = {
  title: { left: 16, top: 12, width: 150, height: 44 },
  instrument: { left: 166, top: 19, width: 220, height: 30 },
  presetBar: { left: 408, top: 19, width: 460, height: 30 },
  readout: { left: 890, top: 12, width: 150, height: 44 },
  meter: { left: 1050, top: 14, width: 14, height: 40 },
  envelopePanel: { left: 16, top: 70, width: 790, height: 396 },
  envelopeEditor: { left: 24, top: 78, width: 774, height: 380 },
  global: { left: 824, top: 70, width: 240, height: 230 },
  pad: { left: 824, top: 310, width: 240, height: 156 },
  panelRow: { left: 16, top: 478, width: 1048, height: 120 },
} satisfies Record<string, Box>;

const PANEL_GAP = 10;

// By DrumEnvelopes::Index; the first curve is the one shown on switching to the instrument
const CURVES: Curve[][] = [
  [
    { envelope: 0, name: 'Pitch', colour: 'var(--color-accent)' },
    { envelope: 1, name: 'Amp', colour: 'var(--color-accent-alt)' },
  ],
  [
    { envelope: 2, name: 'Pitch', colour: 'var(--color-accent)' },
    { envelope: 3, name: 'Body', colour: 'var(--color-accent-alt)' },
    { envelope: 4, name: 'Noise', colour: 'var(--color-accent-alt2)' },
  ],
  [
    { envelope: 5, name: 'Metal', colour: 'var(--color-accent-alt)' },
    { envelope: 6, name: 'Noise', colour: 'var(--color-accent-alt2)' },
  ],
];

interface PanelSpec {
  title: string;
  // The parameter, and the label if not its name. A choice parameter is a Toggle, the rest Knobs
  controls: [id: string, label?: string][];
  header?: { id: string; width: number };
  columns?: number;
}

const GLOBAL: PanelSpec = { title: 'Global', columns: 2, controls: [['tune'], ['keyTrack'], ['length'], ['pitchDepth']] };

// The cymbal has no pitch envelope for these to act on
const PITCH_IDS = ['keyTrack', 'pitchDepth'];

const DRIVE: PanelSpec = { title: 'Drive', header: { id: 'driveType', width: 72 }, controls: [['driveAmount', 'Amount'], ['driveMix', 'Mix']] };
const EQ: PanelSpec = { title: 'EQ', controls: [['eqLowGain'], ['eqMidFreq'], ['eqMidGain'], ['eqHighGain']] };
const OUTPUT: PanelSpec = { title: 'Output', controls: [['clipAmount'], ['outputGain', 'Gain']] };

// AudioPluginAudioProcessorEditor::getSoundPanels, by instrument
const SOUND_PANELS: PanelSpec[][] = [
  [
    { title: 'Sub', controls: [['subLevel', 'Level'], ['subHarmonics'], ['subPhase']] },
    { title: 'Click', header: { id: 'clickType', width: 84 }, controls: [['clickLevel', 'Level'], ['clickTone', 'Tone'], ['clickDecay', 'Decay'], ['clickPitch', 'Pitch']] },
    DRIVE, EQ, OUTPUT,
  ],
  [
    { title: 'Body', controls: [['snareBodyLevel', 'Level'], ['snareBodyHarmonics', 'Harmonics']] },
    { title: 'Noise', controls: [['snareNoiseLevel', 'Level'], ['snareNoiseLowCut', 'Low Cut'], ['snareNoiseHighCut', 'High Cut']] },
    { title: 'Snap', header: { id: 'snareSnapType', width: 84 }, controls: [['snareSnapLevel', 'Level'], ['snareSnapTone', 'Tone'], ['snareSnapDecay', 'Decay'], ['snareSnapPitch', 'Pitch']] },
    DRIVE, EQ, OUTPUT,
  ],
  [
    { title: 'Metal', controls: [['cymbalMetalLevel', 'Level'], ['cymbalMetalTone', 'Tone'], ['cymbalMetalRing', 'Ring']] },
    { title: 'Noise', controls: [['cymbalNoiseLevel', 'Level'], ['cymbalNoiseLowCut', 'Low Cut'], ['cymbalNoiseHighCut', 'High Cut']] },
    DRIVE, EQ, OUTPUT,
  ],
];

// Every parameter the preview depends on
const PREVIEW_IDS = [
  'instrument',
  ...new Set([GLOBAL, ...SOUND_PANELS.flat()].flatMap(({ controls, header }) => [...controls.map(([id]) => id), ...(header ? [header.id] : [])])),
];

// Nearest note, with middle C as C4 (so 43.65 Hz is F1), as juce::MidiMessage::getMidiNoteName
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
export const noteName = (hz: number) => {
  const note = Math.round(69 + 12 * Math.log2(hz / 440));
  return `${NOTE_NAMES[((note % 12) + 12) % 12]}${Math.floor(note / 12) - 1}`;
};

const percent = (gain: number) => `${Math.round(gain * 100)}%`;

// Pitch envelopes read in Hz and note names, level envelopes as percentages
const useFormats = (backend: DrumsBackend) => {
  const isPitch = useCallback((envelope: number) => backend.envelopes[envelope]?.scale === 'logarithmic', [backend]);

  const formatValue = useCallback((envelope: number, value: number) => (isPitch(envelope) ? `${hertzText(value)}  ${noteName(value)}` : percent(value)), [isPitch]);

  const formatGridValue = useCallback(
    (envelope: number, value: number) => {
      if (!isPitch(envelope)) return percent(value);
      return value >= 1000 ? `${Math.round(value / 1000)}k` : `${Math.round(value)}`;
    },
    [isPitch]
  );

  return { formatValue, formatGridValue };
};

// Panel minimum widths measure the title, so lay out again once Orbitron has loaded
function useFontsLoaded() {
  const [loaded, setLoaded] = useState(false);
  useEffect(() => {
    let cancelled = false;
    document.fonts?.ready.then(() => !cancelled && setLoaded(true));
    return () => {
      cancelled = true;
    };
  }, []);
  return loaded;
}

// Re-renders the hit when the sound, the note or the view changes
function usePreview(backend: DrumsBackend, note: number, viewSeconds: number) {
  const [preview, setPreview] = useState<Preview | null>(null);
  const [changes, setChanges] = useState(0);

  useEffect(() => {
    const changed = () => setChanges((count) => count + 1);
    const unsubscribes = [
      ...PREVIEW_IDS.map((id) => backend.subscribe(id, changed)),
      ...backend.envelopes.map((_, index) => backend.subscribeEnvelope(index, changed)),
    ];
    return () => unsubscribes.forEach((unsubscribe) => unsubscribe());
  }, [backend]);

  useEffect(() => {
    let cancelled = false;
    const frame = requestAnimationFrame(() => {
      backend.preview(note, Math.min(viewSeconds, MAX_PREVIEW_SECONDS)).then((next) => !cancelled && setPreview(next));
    });
    return () => {
      cancelled = true;
      cancelAnimationFrame(frame);
    };
  }, [backend, note, viewSeconds, changes]);

  return preview;
}

function ParamPanel({ backend, spec, box, hasPitch }: { backend: DrumsBackend; spec: PanelSpec; box: Box; hasPitch: boolean }) {
  const layout = panelLayout(box, spec.controls.length, { columns: spec.columns, headerWidth: spec.header?.width });

  return (
    <>
      <Panel title={spec.title} box={box} />
      {spec.header && <ChoiceBox backend={backend} id={spec.header.id} box={layout.header} />}
      {spec.controls.map(([id, label], i) => {
        const Control = backend.param(id).kind === 'choice' ? Toggle : Knob;
        return <Control key={id} backend={backend} id={id} label={label} box={layout.controls[i]!} enabled={hasPitch || !PITCH_IDS.includes(id)} />;
      })}
    </>
  );
}

function Readout({ preview }: { preview: Preview | null }) {
  if (!preview || preview.tailHz <= 0) return null;
  const { left, top, width } = BOXES.readout;
  const line = (y: number, height: number) => ({ left, top: top + y, width, height });

  return (
    <>
      <Text style={line(0, 16)} size={11} bold colour="dim" right>
        TAIL
      </Text>
      <Text style={line(16, 22)} size={18} bold colour="accent" right>
        {noteName(preview.tailHz)}
      </Text>
      <Text style={line(38, 6)} size={11} colour="dim" right>
        {hertzText(preview.tailHz)}
      </Text>
    </>
  );
}

export const Editor = memo(function Editor({ backend }: { backend: DrumsBackend }) {
  const { prompt, overlay } = usePrompt();
  const { value: instrumentValue } = useParam(backend, 'instrument');
  const instrument = Math.round(instrumentValue);
  const { value: length } = useParam(backend, 'length');
  const hits = useHits(backend);
  const fontsLoaded = useFontsLoaded();
  const { formatValue, formatGridValue } = useFormats(backend);
  const curves = CURVES[instrument]!;
  const [envelope, setEnvelope] = useState(curves[0]!.envelope);
  const [view, setView] = useState(0.5);

  // Switching instrument edits its first curve
  useEffect(() => setEnvelope(CURVES[instrument]![0]!.envelope), [instrument]);

  // Where the last note played (and the Hit pad) settles; the instrument keeps its own
  const note = backend.lastNote();
  const preview = usePreview(backend, note, view);

  const panels = SOUND_PANELS[instrument]!;
  const rowBoxes = layOutRow(
    panels.map((panel) => ({ numControls: panel.controls.length, minimumWidth: minimumWidth(panel.title, panel.header?.width) })),
    BOXES.panelRow,
    PANEL_GAP
  );

  const hasPitch = instrument !== 2;

  return (
    <div className="relative overflow-hidden bg-background" style={{ width: EDITOR_WIDTH, height: EDITOR_HEIGHT }} data-fonts={fontsLoaded}>
      <Text style={{ ...BOXES.title, height: 16 }} size={11} bold colour="dim" bottom>
        SAMPLEREALM
      </Text>
      <Text style={{ ...BOXES.title, top: BOXES.title.top + 16, height: 28 }} size={22} bold>
        DRUMS
      </Text>
      <ChoiceButtons backend={backend} id="instrument" box={BOXES.instrument} />
      <PresetBar presets={backend.presets} prompt={prompt} box={BOXES.presetBar} />
      <Readout preview={preview} />
      <LevelMeter backend={backend} box={BOXES.meter} />

      <Panel box={BOXES.envelopePanel} />
      <EnvelopeEditor
        backend={backend}
        box={BOXES.envelopeEditor}
        curves={curves}
        envelope={curves.some((curve) => curve.envelope === envelope) ? envelope : curves[0]!.envelope}
        onSelect={setEnvelope}
        timeScale={Math.max(length, 0.01)}
        preview={preview}
        formatValue={formatValue}
        formatGridValue={formatGridValue}
        onViewChange={setView}
      />

      <ParamPanel backend={backend} spec={GLOBAL} box={BOXES.global} hasPitch={hasPitch} />
      <TriggerPad text="Hit" box={BOXES.pad} hits={hits} onTrigger={backend.audition} />

      {panels.map((spec, i) => (
        <ParamPanel key={`${instrument}-${spec.title}`} backend={backend} spec={spec} box={rowBoxes[i]!} hasPitch={hasPitch} />
      ))}

      {overlay}
    </div>
  );
});
