// The built engine, played as the demo's worklet plays it
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { parseState } from '@samplerealm/modules/demo/workletBackend.ts';
import { blockSize, sampleRate } from '@samplerealm/modules/testing/engine.ts';
import { parseEnvelopes } from '../src/demo/demoBackend.ts';
import { renderPreview } from '../src/demo/engine.ts';
import { drums, engine, info } from './engine.ts';

const noteOn = 0x90;
const kick = 0;
const snare = 1;
const cymbal = 2;

const peak = (samples: ArrayLike<number>) => Array.from(samples).reduce((max, sample) => Math.max(max, Math.abs(sample)), 0);

const frequency = (samples: ArrayLike<number>) => {
  let rising = 0;
  for (let i = 1; i < samples.length; i++) if (samples[i - 1]! < 0 && samples[i]! >= 0) rising++;
  return rising / (samples.length / sampleRate);
};

const seconds = (duration: number) => Math.round(duration * sampleRate);

const named = (id: string) => info.params.find((p) => p.id === id);
const textOf = (id: string) => drums.text[info.params.findIndex((p) => p.id === id)];

//==============================================================================
test('the ui info carries the version, the commit, the build type and every parameter in host order', () => {
  assert.match(info.version, /^\d+\.\d+\.\d+$/);
  assert.match(info.commit, /^([0-9a-f]{7,}(-local)?|unknown)$/);
  assert.match(info.build, /^(Debug|Release)$/);
  assert.deepEqual(info.params.map((p) => p.id), [
    'instrument', 'outputGain', 'tune', 'keyTrack', 'length', 'pitchDepth',
    'subLevel', 'subHarmonics', 'subPhase',
    'clickType', 'clickLevel', 'clickTone', 'clickDecay', 'clickPitch',
    'snareBodyLevel', 'snareBodyHarmonics',
    'snareNoiseLevel', 'snareNoiseLowCut', 'snareNoiseHighCut',
    'snareSnapType', 'snareSnapLevel', 'snareSnapTone', 'snareSnapDecay', 'snareSnapPitch',
    'cymbalMetalLevel', 'cymbalMetalTone', 'cymbalMetalRing',
    'cymbalNoiseLevel', 'cymbalNoiseLowCut', 'cymbalNoiseHighCut',
    'driveType', 'driveAmount', 'driveMix',
    'eqLowGain', 'eqMidFreq', 'eqMidGain', 'eqHighGain',
    'clipAmount',
  ]);

  // The unit is part of each value's text, so none is given
  assert.deepEqual(named('clickDecay'), {
    id: 'clickDecay', name: 'Click Decay', kind: 'real', min: 1, max: 100, defaultValue: 12, step: 0.1, skew: 0.354358, unit: '', choices: [],
  });
  assert.deepEqual(named('instrument'), {
    id: 'instrument', name: 'Instrument', kind: 'choice', min: 0, max: 2, defaultValue: 0, step: 1, skew: 1, unit: '', choices: ['Kick', 'Snare', 'Cymbal'],
  });
});

test('the drum info carries how each value reads and every envelope', () => {
  assert.equal(drums.minusInfinityDb, -60);
  assert.equal(drums.text.length, info.params.length);
  assert.deepEqual(textOf('clickDecay'), { format: 'suffix', suffix: ' ms', decimals: 1 });
  assert.deepEqual(textOf('instrument'), { format: 'suffix', suffix: '', decimals: 0 });
  assert.equal(textOf('subPhase')?.suffix, '°');
  assert.equal(textOf('eqMidFreq')?.format, 'hertz');
  assert.equal(textOf('outputGain')?.format, 'decibels');

  assert.deepEqual(drums.envelopes.map((e) => [e.id, e.scale, e.nodes.length]), [
    ['pitch', 'logarithmic', 5], ['amp', 'linear', 4], ['snarePitch', 'logarithmic', 4], ['snareBody', 'linear', 3],
    ['snareNoise', 'linear', 3], ['cymbalMetal', 'linear', 2], ['cymbalNoise', 'linear', 3],
  ]);
  assert.deepEqual(drums.envelopes[0]!.nodes.at(-1), { time: 0.35, value: 43.65, curve: 0 });
  assert.equal(drums.envelopes[5]!.maxTime, 4);
});

test('there is silence until a note arrives', () => {
  const e = engine();
  assert.equal(peak(e.play(0.2)), 0);
  assert.equal(e.engine.hitCount(), 0);
});

test('each instrument sounds on a note, finite, and dies away', () => {
  for (const instrument of [kick, snare, cymbal]) {
    const e = engine();
    e.set('instrument', instrument);
    e.engine.midi(noteOn, 40, 100);

    const hit = e.play(0.1);
    assert.ok(hit.every(Number.isFinite), `instrument ${instrument}`);
    assert.ok(peak(hit) > 0.1, `instrument ${instrument}: peak ${peak(hit)}`);

    e.play(1);
    assert.ok(peak(e.play(0.1)) < 1e-4, `instrument ${instrument} still sounds`);
    assert.equal(e.engine.hitCount(), 1);
  }
});

test('a note-on of velocity 0 is not a hit', () => {
  const e = engine();
  e.engine.midi(noteOn, 40, 0);
  assert.equal(peak(e.play(0.1)), 0);
  assert.equal(e.engine.hitCount(), 0);
});

// The voices stop within a few ms; the DC blocker and EQ then ring down
test('switching instrument fades out the old one', () => {
  const after = (instrument: number) => {
    const e = engine();
    e.engine.midi(noteOn, 29, 100);
    e.play(0.05);
    e.set('instrument', instrument);
    e.play(0.01);
    return peak(e.play(0.05));
  };

  assert.ok(after(snare) < 0.1 * after(kick), `${after(snare)} against ${after(kick)}`);
});

test('Key Track puts the tail on the played note', () => {
  const tail = (note: number, keyTrack: number) => {
    const e = engine();
    e.set('keyTrack', keyTrack);
    e.set('clickLevel', -60);
    e.set('driveAmount', 0);
    e.engine.midi(noteOn, note, 100);
    return frequency(e.play(0.3).slice(seconds(0.15)));
  };

  const ratio = tail(41, 1) / tail(29, 1);
  assert.ok(Math.abs(ratio - 2) < 0.15, `octave up gave ${ratio}`);
  assert.ok(Math.abs(tail(41, 0) / tail(29, 0) - 1) < 0.05);
});

test('audition replays the last note of the current instrument', () => {
  const e = engine();
  assert.equal(e.engine.lastNote(), 29);

  e.engine.midi(noteOn, 33, 100);
  e.play(0.6);
  e.engine.audition();

  assert.ok(peak(e.play(0.05)) > 0.1);
  assert.equal(e.engine.hitCount(), 2);
  assert.equal(e.engine.lastNote(), 33);

  e.set('instrument', snare);
  assert.equal(e.engine.lastNote(), 55);
});

test('a shorter amp envelope shortens the hit', () => {
  const tailPeak = (amp?: { time: number; value: number; curve: number }[]) => {
    const e = engine();
    e.set('clickLevel', -60);
    if (amp) e.envelope('amp', amp);
    e.engine.midi(noteOn, 29, 100);
    return peak(e.play(0.2).slice(seconds(0.1)));
  };

  assert.ok(tailPeak() > 0.1);
  assert.ok(tailPeak([{ time: 0, value: 1, curve: 0 }, { time: 0.05, value: 0, curve: 0 }]) < 1e-3);
});

test('blocks are clamped to the engine size', () => {
  const e = engine();
  e.engine.midi(noteOn, 29, 100);
  assert.ok(e.block(blockSize).every(Number.isFinite));
  assert.doesNotThrow(() => e.engine.render(100000));
  assert.doesNotThrow(() => e.engine.render(-1));
});

test('the meter reads the loudest sample since it was last read', () => {
  const e = engine();
  assert.equal(e.engine.peak(0), 0);

  e.engine.midi(noteOn, 29, 100);
  const hit = e.play(0.1);

  assert.ok(Math.abs(e.engine.peak(0) - peak(hit)) < 1e-6);
  assert.equal(e.engine.peak(0), 0);
  assert.equal(e.engine.peak(1), 0);
});

test('the preview renders one hit and where its pitch settles', () => {
  const e = engine();
  const kickHit = renderPreview(e.engine, 29, 8);

  assert.equal(kickHit.sampleRate, 44100);
  assert.ok(Math.abs(kickHit.samples.length - Math.round(0.353 * 44100)) < 50, `${kickHit.samples.length} samples`);
  assert.ok(Math.abs(kickHit.tailHz - 43.65) < 0.01);
  assert.ok(peak(kickHit.samples) > 0.1);

  assert.equal(renderPreview(e.engine, 29, 0.1).samples.length, 4410);

  e.set('tune', 12);
  assert.ok(Math.abs(renderPreview(e.engine, 29, 8).tailHz - 87.3) < 0.01);

  e.set('instrument', cymbal);
  assert.equal(renderPreview(e.engine, 42, 8).tailHz, 0);
});

//==============================================================================
interface Scenario {
  name: string;
  preset?: string;
  params: Record<string, number>;
  blocks: number;
  notes: { block: number; offset: number; note: number }[];
  fingerprint: number[][];
}

interface Golden {
  sampleRate: number;
  blockSize: number;
  blocksPerFrame: number;
  scenarios: Scenario[];
}

// Assets/Presets as PresetManager applies them: unlisted parameters and envelopes take their defaults
const readPreset = (name: string) => {
  const xml = readFileSync(new URL(`../../Assets/Presets/${name}.srdrum`, import.meta.url), 'utf8');
  return { values: parseState(xml).values, envelopes: parseEnvelopes(xml) };
};

// Emscripten's libm rounds differently in the last bit from macOS's
test('renders the same as the plugin, to within libm rounding', () => {
  const golden = JSON.parse(readFileSync(new URL('../../Tests/fixtures/golden.json', import.meta.url), 'utf8')) as Golden;
  const { blockSize: size, blocksPerFrame } = golden;

  for (const scenario of golden.scenarios) {
    const e = engine(null);
    const preset = scenario.preset ? readPreset(scenario.preset) : { values: {}, envelopes: {} };

    for (const [id, value] of Object.entries({ ...preset.values, ...scenario.params })) e.set(id, value);
    for (const [id, nodes] of Object.entries(preset.envelopes)) e.envelope(id, nodes);

    e.engine.prepare(golden.sampleRate, 1);

    const fingerprint: number[][] = [];
    let sumOfSquares = 0;
    let framePeak = 0;

    for (let block = 0; block < scenario.blocks; block++) {
      const notes = scenario.notes.filter((note) => note.block === block);

      // Notes start at the top of the engine's block, so split it where the plugin's starts
      let position = 0;
      const samples: number[] = [];
      for (const { offset, note } of notes) {
        if (offset > position) samples.push(...e.block(offset - position));
        position = offset;
        e.engine.midi(noteOn, note, 100);
      }
      samples.push(...e.block(size - position));

      for (const sample of samples) {
        sumOfSquares += Math.fround(sample * sample);
        framePeak = Math.max(framePeak, Math.abs(sample));
      }

      if ((block + 1) % blocksPerFrame === 0) {
        fingerprint.push([Math.sqrt(sumOfSquares / (size * blocksPerFrame)), framePeak]);
        sumOfSquares = 0;
        framePeak = 0;
      }
    }

    assert.equal(fingerprint.length, scenario.fingerprint.length, scenario.name);

    const firstMismatch = fingerprint.findIndex((frame, i) =>
      frame.some((value, j) => Math.abs(value - scenario.fingerprint[i]![j]!) > 1e-5 + 5e-3 * Math.abs(scenario.fingerprint[i]![j]!)));

    assert.equal(firstMismatch, -1, `${scenario.name}: frame ${firstMismatch} is ${fingerprint[firstMismatch]}, expected ${scenario.fingerprint[firstMismatch]}`);
  }
});
