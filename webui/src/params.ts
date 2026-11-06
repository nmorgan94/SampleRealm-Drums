// Mirrors Source/params/ParameterFormats.h and juce::NormalisableRange
import type { ParamInfo } from './backend.ts';

// ParameterSpecs::minusInfinityDb
export const MINUS_INFINITY_DB = -60;

const clamp01 = (value: number) => Math.min(1, Math.max(0, value));

export const fromProportion = (param: ParamInfo, proportion: number) => {
  const p = clamp01(proportion);
  const value = param.min + (param.max - param.min) * (param.skew === 1 ? p : Math.pow(p, 1 / param.skew));
  return snap(param, value);
};

export const toProportion = (param: ParamInfo, value: number) => {
  const p = clamp01((value - param.min) / (param.max - param.min));
  return param.skew === 1 ? p : Math.pow(p, param.skew);
};

export const snap = (param: ParamInfo, value: number) => {
  const clamped = Math.min(param.max, Math.max(param.min, value));
  if (param.step <= 0) return clamped;
  // AudioParameterChoice rounds with roundToInt; NormalisableRange::snapToLegalValue rounds halves up
  const steps = (clamped - param.min) / param.step;
  const snapped = param.min + (param.kind === 'choice' ? roundToInt(steps) : Math.round(steps)) * param.step;
  // Avoids 0.7000000000000001
  return Math.min(param.max, Number(snapped.toFixed(decimalPlaces(param.step))));
};

const decimalPlaces = (step: number) => {
  if (step <= 0) return 7;
  if (Number.isInteger(step)) return 0;

  let places = 7;
  let scaled = Math.abs(Math.round(step * 10 ** places));
  while (scaled % 10 === 0 && places > 0) {
    places--;
    scaled /= 10;
  }
  return places;
};

// juce::roundToInt, which rounds halves to even
const roundToInt = (value: number) => {
  const floor = Math.floor(value);
  const fraction = value - floor;
  if (fraction !== 0.5) return Math.round(value);
  return floor % 2 === 0 ? floor : floor + 1;
};

// srd::params::number
const number = (value: number, decimals: number) => {
  if (decimals <= 0) return `${roundToInt(value) || 0}`;
  const text = value.toFixed(decimals);
  return /^[-0.]+$/.test(text) ? text.replace('-', '') : text;
};

export const hertzText = (hz: number) => (hz >= 1000 ? `${number(hz / 1000, 2)} kHz` : `${number(hz, hz < 100 ? 1 : 0)} Hz`);

export const formatValue = (param: ParamInfo, value: number) => {
  if (param.kind === 'choice') return param.choices[Math.round(value)] ?? '';

  switch (param.format) {
    case 'decibels':
      return value <= MINUS_INFINITY_DB + 0.05 ? '-inf dB' : `${number(value, 1)} dB`;
    case 'hertz':
      return hertzText(value);
    case 'suffix':
      return number(value, param.decimals) + param.suffix;
  }
};

// The leading number, as juce::String::getFloatValue reads it
const leadingNumber = (text: string) => parseFloat(text.trim());

export const parseValue = (param: ParamInfo, text: string): number | null => {
  if (param.kind === 'choice') {
    const index = param.choices.indexOf(text.trim());
    return index < 0 ? null : index;
  }

  if (param.format === 'decibels' && /inf/i.test(text)) return snap(param, MINUS_INFINITY_DB);

  const value = leadingNumber(text);
  if (Number.isNaN(value)) return null;

  return snap(param, param.format === 'hertz' && /k/i.test(text) ? value * 1000 : value);
};
