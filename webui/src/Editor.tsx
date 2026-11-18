// Mirrors Source/PluginEditor.cpp, with the boxes resized() gives
import { memo, useCallback, useEffect, useState, type CSSProperties } from 'react';
import { EditorRootContext } from '@samplerealm/modules/editor/editorRoot.ts';
import { HeaderBar, type HeaderLayout } from '@samplerealm/modules/editor/HeaderBar.tsx';
import { useParam } from '@samplerealm/modules/editor/hooks.ts';
import { centred, type Rect } from '@samplerealm/modules/editor/layout.ts';
import { LevelMeter } from '@samplerealm/modules/editor/LevelMeter.tsx';
import { useManual } from '@samplerealm/modules/editor/ManualOverlay.tsx';
import { usePrompt } from '@samplerealm/modules/editor/PromptOverlay.tsx';
import manualText from '../../Assets/Manual.md?raw';
import type { DrumsBackend, Loop, Preview } from './backend.ts';
import { ChoiceBox, ChoiceButtons, Knob, Select, TextButton, Toggle } from './controls.tsx';
import { EnvelopeEditor, type Curve } from './EnvelopeEditor.tsx';
import { useHits } from './hooks.ts';
import { layOutRow } from './layout.ts';
import { minimumWidth, Panel, panelLayout } from './Panel.tsx';
import { hertzText } from './params.ts';
import { Text } from './Text.tsx';
import { StepRow } from './StepRow.tsx';
import { TriggerPad } from './TriggerPad.tsx';

export const EDITOR_WIDTH = 1080;
export const EDITOR_HEIGHT = 610;

const MAX_PREVIEW_SECONDS = 8;

// AudioPluginAudioProcessorEditor::titleLayout
const TITLE_LAYOUT = { textWidth: 150, largeHeight: 22, inset: 12, switchWidth: 220, switchHeight: 30 };

// Source/ui/HeaderTitle.h
const SMALL_LINE_HEIGHT = 16;
const SMALL_FONT_HEIGHT = 11;

// Source/ui/TailMeter.h
const TAIL_METER = { meterWidth: 27, meterHeight: 40, gap: 10, readoutWidth: 137 };

// AudioPluginAudioProcessorEditor::headerLayout
const HEADER_LAYOUT: HeaderLayout = {
  height: 68,
  edge: 16,
  titleWidth: TITLE_LAYOUT.textWidth + TITLE_LAYOUT.switchWidth,
  titleHeight: TITLE_LAYOUT.largeHeight,
  gap: 22,
  controlsHeight: 30,
  meterWidth: TAIL_METER.readoutWidth + TAIL_METER.gap + TAIL_METER.meterWidth,
  meterInset: 12,
};

// The level meter's bar and background, as CustomLookAndFeel gives LevelMeter's colour ids
const METER_COLOURS = { '--color-thumb': 'var(--color-accent-alt)', '--color-meter-background': 'var(--color-panel)' } as CSSProperties;

const BOXES = {
  envelopePanel: { left: 16, top: 70, width: 790, height: 396 },
  envelopeEditor: { left: 24, top: 78, width: 774, height: 380 },
  global: { left: 824, top: 70, width: 240, height: 230 },
  auditionLabel: { left: 824, top: 310, width: 240, height: 12 },
  pad: { left: 824, top: 326, width: 240, height: 82 },
  steps: { left: 824, top: 414, width: 240, height: 20 },
  loopPattern: { left: 824, top: 440, width: 154, height: 26 },
  loopButton: { left: 984, top: 440, width: 80, height: 26 },
  panelRow: { left: 16, top: 478, width: 1048, height: 120 },
} satisfies Record<string, Rect>;

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
  // The parameter, and the label if not its name
  controls: [id: string, label?: string][];
  // A choice box, or a Toggle
  header?: { id: string; width: number; inset?: number; toggle?: boolean };
  columns?: number;
}

const GLOBAL: PanelSpec = { title: 'Global', columns: 2, header: { id: 'keyTrack', width: 76, inset: 19, toggle: true }, controls: [['tune'], ['length'], ['pitchDepth'], ['velocity']] };

// The cymbal has no pitch envelope for these to act on
const PITCH_IDS = ['keyTrack', 'pitchDepth'];

const DRIVE: PanelSpec = { title: 'Drive', header: { id: 'driveType', width: 72 }, controls: [['driveAmount', 'Amount'], ['driveMix', 'Mix']] };
const EQ: PanelSpec = { title: 'EQ', controls: [['eqLowGain'], ['eqMidFreq'], ['eqMidGain'], ['eqHighGain']] };
const OUTPUT: PanelSpec = { title: 'Output', controls: [['clipAmount'], ['outputGain', 'Gain']] };
const CYMBAL_OUTPUT: PanelSpec = { title: 'Output', controls: [['clipAmount'], ['cymbalWidth'], ['outputGain', 'Gain']] };

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
    DRIVE, EQ, CYMBAL_OUTPUT,
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

function ParamPanel({ backend, spec, box, hasPitch }: { backend: DrumsBackend; spec: PanelSpec; box: Rect; hasPitch: boolean }) {
  const layout = panelLayout(box, spec.controls.length, { columns: spec.columns, headerWidth: spec.header?.width, headerInset: spec.header?.inset });
  const enabled = (id: string) => hasPitch || !PITCH_IDS.includes(id);
  const Header = spec.header?.toggle ? Toggle : ChoiceBox;

  return (
    <>
      <Panel title={spec.title} box={box} />
      {spec.header && <Header backend={backend} id={spec.header.id} box={layout.header} enabled={enabled(spec.header.id)} />}
      {spec.controls.map(([id, label], i) => (
        <Knob key={id} backend={backend} id={id} label={label} box={layout.controls[i]!} enabled={enabled(id)} />
      ))}
    </>
  );
}

// Holds the hits, so one redraws the pad and not the editor
function HitPad({ backend }: { backend: DrumsBackend }) {
  return <TriggerPad text="Hit" box={BOXES.pad} hits={useHits(backend)} onTrigger={backend.audition} />;
}

// The audition loop's pattern and switch, under the pad
function LoopControls({ backend }: { backend: DrumsBackend }) {
  const [loop, setLoop] = useState(backend.getLoop);

  const change = (next: Loop) => {
    setLoop(next);
    backend.setLoop(next);
  };

  return (
    <>
      <Select
        label="Loop pattern"
        choices={backend.patterns}
        current={loop.pattern}
        onSelect={(pattern) => change({ ...loop, pattern })}
        box={BOXES.loopPattern}
      />
      <TextButton text="Loop" toggled={loop.on} style={BOXES.loopButton} onClick={() => change({ ...loop, on: !loop.on })} />
    </>
  );
}

function Readout({ preview, box }: { preview: Preview | null; box: Rect }) {
  if (!preview || preview.tailHz <= 0) return null;
  const { left, top, width } = box;
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

// TailMeter::resized
function TailMeter({ backend, preview, box }: { backend: DrumsBackend; preview: Preview | null; box: Rect }) {
  const { meterWidth, meterHeight, readoutWidth } = TAIL_METER;

  return (
    <>
      <Readout preview={preview} box={{ ...box, width: readoutWidth }} />
      <div className="contents" style={METER_COLOURS}>
        <LevelMeter
          backend={backend}
          x={box.left + box.width - meterWidth}
          y={box.top + centred(box.height, meterHeight)}
          width={meterWidth}
          height={meterHeight}
        />
      </div>
    </>
  );
}

// HeaderTitle::paint and resized: small above large, the instrument switch to their right
function HeaderTitle({ backend, bounds }: { backend: DrumsBackend; bounds: Rect }) {
  const { textWidth, largeHeight, inset, switchWidth, switchHeight } = TITLE_LAYOUT;
  const text = { left: bounds.left, top: bounds.top + inset, width: textWidth, height: bounds.height - 2 * inset };
  const switchBox = { left: bounds.left + textWidth, top: bounds.top + centred(bounds.height, switchHeight), width: switchWidth, height: switchHeight };

  return (
    <>
      <h1 aria-label="SAMPLEREALM DRUMS">
        <Text style={{ ...text, height: SMALL_LINE_HEIGHT }} size={SMALL_FONT_HEIGHT} bold colour="dim" bottom>
          SAMPLEREALM
        </Text>
        <Text style={{ ...text, top: text.top + SMALL_LINE_HEIGHT, height: text.height - SMALL_LINE_HEIGHT }} size={largeHeight} bold>
          DRUMS
        </Text>
      </h1>
      <ChoiceButtons backend={backend} id="instrument" box={switchBox} />
    </>
  );
}

export const Editor = memo(function Editor({ backend }: { backend: DrumsBackend }) {
  const [root, setRoot] = useState<HTMLDivElement | null>(null);
  const { prompt, overlay } = usePrompt();
  const manual = useManual(manualText);
  const { value: instrumentValue } = useParam(backend, 'instrument');
  const instrument = Math.round(instrumentValue);
  const { value: length } = useParam(backend, 'length');
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
    panels.map((panel) => ({ numControls: panel.controls.length, minimumWidth: minimumWidth(panel.title, panel.header?.width, panel.header?.inset) })),
    BOXES.panelRow,
    PANEL_GAP
  );

  const hasPitch = instrument !== 2;

  return (
    <EditorRootContext.Provider value={root}>
    <div ref={setRoot} className="relative overflow-hidden bg-background select-none" style={{ width: EDITOR_WIDTH, height: EDITOR_HEIGHT }} data-fonts={fontsLoaded}>
      <HeaderBar
        title={(bounds) => <HeaderTitle backend={backend} bounds={bounds} />}
        width={EDITOR_WIDTH}
        layout={HEADER_LAYOUT}
        backend={backend}
        prompt={prompt}
        showManual={manual.showManual}
        meter={(box) => <TailMeter backend={backend} preview={preview} box={box} />}
      />

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
      <Text style={BOXES.auditionLabel} size={11} bold colour="dim">
        AUDITION
      </Text>
      <HitPad backend={backend} />
      <StepRow backend={backend} box={BOXES.steps} readout={BOXES.auditionLabel} />
      <LoopControls backend={backend} />

      {panels.map((spec, i) => (
        <ParamPanel key={`${instrument}-${spec.title}`} backend={backend} spec={spec} box={rowBoxes[i]!} hasPitch={hasPitch} />
      ))}

      {overlay}
      {manual.overlay}
    </div>
    </EditorRootContext.Provider>
  );
});
