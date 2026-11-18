// The built engine, played as the demo's worklet plays it
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { parseState } from '@samplerealm/modules/demo/workletBackend.ts';
import { blockSize, sampleRate } from '@samplerealm/modules/testing/engine.ts';
import { toggledStep } from '../src/backend.ts';
import { parseEnvelopes } from '../src/demo/demoBackend.ts';
import { readLoopSteps, renderPreview } from '../src/demo/engine.ts';
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
    'instrument', 'outputGain', 'tune', 'keyTrack', 'length', 'pitchDepth', 'velocity',
    'subLevel', 'subHarmonics', 'subPhase',
    'clickType', 'clickLevel', 'clickTone', 'clickDecay', 'clickPitch',
    'snareBodyLevel', 'snareBodyHarmonics',
    'snareNoiseLevel', 'snareNoiseLowCut', 'snareNoiseHighCut',
    'snareSnapType', 'snareSnapLevel', 'snareSnapTone', 'snareSnapDecay', 'snareSnapPitch',
    'cymbalMetalLevel', 'cymbalMetalTone', 'cymbalMetalRing',
    'cymbalNoiseLevel', 'cymbalNoiseLowCut', 'cymbalNoiseHighCut', 'cymbalWidth',
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
  assert.deepEqual(drums.patterns, ['Two-step', 'Halftime', 'Four-floor', 'Sixteenths']);
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

test('velocity sets how hard a hit plays, by the Velocity amount', () => {
  // Without Drive, which squashes a hard hit more than a soft one
  const hit = (amount: number, velocity: number) => {
    const e = engine();
    e.set('velocity', amount);
    e.set('driveAmount', 0);
    e.play(0.1);
    e.engine.midi(noteOn, 29, velocity);
    return { peak: peak(e.play(0.05)), level: e.engine.lastHitLevel() };
  };

  assert.ok(hit(100, 40).peak < 0.4 * hit(100, 127).peak);
  assert.ok(Math.abs(hit(100, 40).level - 40 / 127) < 1e-6);
  assert.equal(hit(100, 127).level, 1);

  assert.equal(hit(0, 40).peak, hit(0, 127).peak);
  assert.equal(hit(0, 40).level, 1);
});

test('audition plays at the velocity it is given', () => {
  const e = engine();
  e.set('velocity', 100);
  e.engine.midi(noteOn, 29, 20);
  e.play(0.05);
  assert.ok(Math.abs(e.engine.lastHitVelocity() - 20 / 127) < 1e-6);

  e.engine.audition(1);
  e.play(0.05);
  assert.equal(e.engine.lastHitLevel(), 1);
  assert.equal(e.engine.lastHitVelocity(), 1);

  e.engine.audition(0.5);
  e.play(0.05);
  assert.equal(e.engine.lastHitLevel(), 0.5);
  assert.equal(e.engine.lastHitVelocity(), 0.5);
});

test('the same note plays the same hit every time', () => {
  const difference = (instrument: number) => {
    const e = engine();
    e.set('instrument', instrument);

    const hit = () => {
      e.engine.midi(noteOn, 40, 100);
      const samples = Array.from(e.play(0.05));
      e.play(2);
      return samples;
    };

    const first = hit();
    const second = hit();
    return peak(first.map((sample, i) => sample - second[i]!));
  };

  for (const instrument of [kick, snare, cymbal]) {
    assert.ok(difference(instrument) < 1e-6, `instrument ${instrument}: ${difference(instrument)}`);
  }
});

test('Width spreads the cymbal over two channels that still sum to the mono hit', () => {
  const hit = (instrument: number, width: number) => {
    const e = engine();
    e.set('instrument', instrument);
    e.set('cymbalWidth', width);
    e.play(0.1);
    e.engine.midi(noteOn, 42, 100);
    const [left, right] = e.stereo(0.1);
    return { left: Array.from(left!), right: Array.from(right!) };
  };

  const mono = hit(cymbal, 0);
  assert.deepEqual(mono.left, mono.right);

  const wide = hit(cymbal, 100);
  const apart = peak(wide.left.map((sample, i) => sample - wide.right[i]!));
  const sumError = peak(wide.left.map((sample, i) => sample + wide.right[i]! - 2 * mono.left[i]!));
  assert.ok(apart > 0.05, `the channels differ by ${apart}`);
  assert.ok(sumError < 1e-5, `the sum is off by ${sumError}`);

  const kickHit = hit(kick, 100);
  assert.deepEqual(kickHit.left, kickHit.right);
});

test("the loop plays the current instrument's part from its first step, at the tempo", () => {
  const e = engine();
  e.engine.setTransport(120, false, 0);

  // A bar is 2 s: the kick's two hits, then nothing once it's off
  e.engine.setLoop(0, true);
  e.play(1.9);
  assert.equal(e.engine.hitCount(), 2);

  e.engine.setLoop(0, false);
  e.play(2);
  assert.equal(e.engine.hitCount(), 2);

  // The cymbal plays the hat's eighths
  e.set('instrument', cymbal);
  e.engine.setLoop(0, true);
  e.play(1.9);
  assert.equal(e.engine.hitCount(), 10);
});

test("a step can be set for the current instrument, until a new pattern replaces them", () => {
  const e = engine();
  const steps = () => readLoopSteps(e.engine);
  e.engine.setTransport(120, false, 0);

  assert.deepEqual(steps(), [127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 112, 0, 0, 0, 0, 0]);
  assert.equal(e.engine.playingStep(), -1);

  // A step switched off keeps its velocity as a negative
  assert.equal(toggledStep(0), 127);
  assert.equal(toggledStep(-112), 112);

  e.engine.setLoopStep(4, 64);
  e.engine.setLoopStep(10, toggledStep(112));
  e.engine.setLoopStep(16, 100);
  e.engine.setLoopStep(1, 500);
  assert.deepEqual(steps(), [127, 127, 0, 0, 64, 0, 0, 0, 0, 0, -112, 0, 0, 0, 0, 0]);

  // A bar is 2 s, a step 125 ms. The last of the three hits was at velocity 64, with Vel Sens at 50 %
  e.engine.setLoop(0, true);
  e.play(0.6);
  assert.equal(e.engine.hitCount(), 3);
  assert.ok(Math.abs(e.engine.lastHitLevel() - (1 - 0.5 * (1 - 64 / 127))) < 1e-5);
  assert.equal(e.engine.playingStep(), 4);

  e.engine.setLoop(0, false);
  assert.equal(e.engine.playingStep(), -1);
  assert.equal(steps()[4], 64);

  e.set('instrument', snare);
  assert.deepEqual(steps(), [0, 0, 0, 0, 127, 0, 0, 0, 0, 0, 0, 0, 127, 0, 0, 0]);

  e.set('instrument', kick);
  e.engine.setLoop(1, false);
  assert.deepEqual(steps(), [127, 0, 0, 0, 0, 0, 90, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
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
  e.engine.audition(1);

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

  // Both channels, which are the same without Width
  for (const channel of [0, 1]) {
    assert.ok(Math.abs(e.engine.peak(channel) - peak(hit)) < 1e-6);
    assert.equal(e.engine.peak(channel), 0);
  }

  assert.equal(e.engine.peak(2), 0);
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
