import assert from 'node:assert/strict';
import { test } from 'node:test';
import { formatValue, fromProportion, MINUS_INFINITY_DB, parseValue, toProportion } from '../src/params.ts';
import { info, param } from './engine.ts';

// From the snapshot in Tests/ParameterSnapshotTests.cpp: the text at 0, 0.5 and 1
const pluginText: Record<string, [string, string, string]> = {
  instrument: ['Kick', 'Snare', 'Cymbal'],
  outputGain: ['-inf dB', '-27.0 dB', '6.0 dB'],
  tune: ['-24.00 st', '0.00 st', '24.00 st'],
  keyTrack: ['Off', 'Off', 'On'],
  length: ['0.25x', '1.00x', '4.00x'],
  pitchDepth: ['0 %', '100 %', '200 %'],
  subLevel: ['-inf dB', '-27.0 dB', '6.0 dB'],
  subHarmonics: ['0 %', '50 %', '100 %'],
  subPhase: ['0°', '45°', '90°'],
  clickType: ['Noise', 'Sweep', 'Impulse'],
  clickLevel: ['-inf dB', '-27.0 dB', '6.0 dB'],
  clickTone: ['200 Hz', '3.00 kHz', '16.00 kHz'],
  clickDecay: ['1.0 ms', '15.0 ms', '100.0 ms'],
  clickPitch: ['200 Hz', '2.00 kHz', '10.00 kHz'],
  snareBodyLevel: ['-inf dB', '-27.0 dB', '6.0 dB'],
  snareBodyHarmonics: ['0 %', '50 %', '100 %'],
  snareNoiseLevel: ['-inf dB', '-27.0 dB', '6.0 dB'],
  snareNoiseLowCut: ['100 Hz', '1.50 kHz', '8.00 kHz'],
  snareNoiseHighCut: ['1.00 kHz', '6.00 kHz', '18.00 kHz'],
  snareSnapType: ['Noise', 'Sweep', 'Impulse'],
  snareSnapLevel: ['-inf dB', '-27.0 dB', '6.0 dB'],
  snareSnapTone: ['200 Hz', '3.00 kHz', '16.00 kHz'],
  snareSnapDecay: ['1.0 ms', '15.0 ms', '100.0 ms'],
  snareSnapPitch: ['200 Hz', '2.00 kHz', '10.00 kHz'],
  cymbalMetalLevel: ['-inf dB', '-27.0 dB', '6.0 dB'],
  cymbalMetalTone: ['2.00 kHz', '6.00 kHz', '16.00 kHz'],
  cymbalMetalRing: ['0 %', '50 %', '100 %'],
  cymbalNoiseLevel: ['-inf dB', '-27.0 dB', '6.0 dB'],
  cymbalNoiseLowCut: ['200 Hz', '3.00 kHz', '12.00 kHz'],
  cymbalNoiseHighCut: ['1.00 kHz', '6.00 kHz', '18.00 kHz'],
  driveType: ['Soft', 'Fold', 'Tube'],
  driveAmount: ['0 %', '50 %', '100 %'],
  driveMix: ['0 %', '50 %', '100 %'],
  eqLowGain: ['-12.0 dB', '0.0 dB', '12.0 dB'],
  eqMidFreq: ['100 Hz', '600 Hz', '5.00 kHz'],
  eqMidGain: ['-12.0 dB', '0.0 dB', '12.0 dB'],
  eqHighGain: ['-12.0 dB', '0.0 dB', '12.0 dB'],
  clipAmount: ['0 %', '50 %', '100 %'],
};

// Also from the snapshot: text typed into the parameter, and the value it gives
const typed: [id: string, text: string, value: number][] = [
  ['outputGain', '-inf dB', -60],
  ['clickLevel', '-inf', -60],
  ['tune', '3.5 st', 3.5],
  ['length', '2x', 2],
  ['subPhase', '30', 30],
  ['clickTone', '1.5 kHz', 1500],
  ['snareNoiseLowCut', '2k', 2000],
  ['clickPitch', '800 Hz', 800],
  ['clickDecay', '12 ms', 12],
  ['snareNoiseLevel', '-12.5 dB', -12.5],
  ['keyTrack', 'On', 1],
  ['driveType', 'Hard', 1],
];

test('the engine uses the plugin minus infinity', () => {
  assert.equal(info.minusInfinityDb, MINUS_INFINITY_DB);
});

test('every parameter has a row in pluginText', () => {
  assert.deepEqual(Object.keys(pluginText), info.params.map(({ id }) => id));
});

test('every value reads as the plugin shows it, at the same knob position', () => {
  for (const [id, texts] of Object.entries(pluginText)) {
    texts.forEach((text, i) => {
      const proportion = i / 2;
      assert.equal(formatValue(param(id), fromProportion(param(id), proportion)), text, `${id} at ${proportion}`);
    });
  }
});

test('typed text reads as the plugin reads it', () => {
  for (const [id, text, value] of typed) assert.equal(parseValue(param(id), text), value, `${id}: ${text}`);
});

test('text without a number is ignored, and typed values clamp and snap', () => {
  assert.equal(parseValue(param('outputGain'), 'loud'), null);
  assert.equal(parseValue(param('outputGain'), '99'), 6);
  assert.equal(parseValue(param('outputGain'), '-3.14'), -3.1);
  assert.equal(parseValue(param('instrument'), 'Tom'), null);
});

test('knob positions map to values and back, skewed ranges included', () => {
  const tone = param('clickTone');

  assert.ok(Math.abs(fromProportion(tone, 0.5) - 3000) < 0.5);
  assert.ok(Math.abs(toProportion(tone, 3000) - 0.5) < 1e-4);
  assert.equal(fromProportion(param('outputGain'), 1.5), 6);
  assert.equal(fromProportion(param('outputGain'), -0.5), -60);
});

test('rounding and signs follow juce::String', () => {
  assert.equal(formatValue(param('driveAmount'), 50.5), '50 %');
  assert.equal(formatValue(param('driveAmount'), 51.5), '52 %');
  assert.equal(formatValue(param('tune'), -0.001), '0.00 st');
  assert.equal(formatValue(param('clickTone'), 99.5), '99.5 Hz');
  assert.equal(formatValue(param('outputGain'), -59.96), '-inf dB');
});
