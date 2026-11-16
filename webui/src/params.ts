// Mirrors Parameters::withValueText and Parameters::hertzText
import { formatValue as sharedFormat, parseValue as sharedParse, snap } from '@samplerealm/modules/editor/params.ts';
import { paramLookup, type DrumInfo, type ParamInfo, type UiInfo } from './backend.ts';

// juce::roundToInt, which rounds halves to even
const roundToInt = (value: number) => {
  const floor = Math.floor(value);
  const fraction = value - floor;
  if (fraction !== 0.5) return Math.round(value);
  return floor % 2 === 0 ? floor : floor + 1;
};

const number = (value: number, decimals: number) => {
  if (decimals <= 0) return `${roundToInt(value) || 0}`;
  const text = value.toFixed(decimals);
  return /^[-0.]+$/.test(text) ? text.replace('-', '') : text;
};

export const hertzText = (hz: number) => (hz >= 1000 ? `${number(hz / 1000, 2)} kHz` : `${number(hz, hz < 100 ? 1 : 0)} Hz`);

// The leading number, as juce::String::getFloatValue reads it
const leadingNumber = (text: string) => parseFloat(text.trim());

// How each parameter's value reads and is typed, for DrumsBackend
export function drumText(info: UiInfo, drums: DrumInfo) {
  const textOf = paramLookup(new Map(info.params.map((param, index) => [param.id, drums.text[index]!])));

  const formatValue = (param: ParamInfo, value: number) => {
    if (param.kind !== 'real') return sharedFormat(param, value);

    const { format, suffix, decimals } = textOf(param.id);

    switch (format) {
      case 'decibels':
        return value <= drums.minusInfinityDb + 0.05 ? '-inf dB' : `${number(value, 1)} dB`;
      case 'hertz':
        return hertzText(value);
      case 'suffix':
        return number(value, decimals) + suffix;
    }
  };

  const parseValue = (param: ParamInfo, text: string): number | null => {
    if (param.kind !== 'real') return sharedParse(param, text);

    const { format } = textOf(param.id);
    if (format === 'decibels' && /inf/i.test(text)) return snap(param, drums.minusInfinityDb);

    const value = leadingNumber(text);
    if (Number.isNaN(value)) return null;

    return snap(param, format === 'hertz' && /k/i.test(text) ? value * 1000 : value);
  };

  return { formatValue, parseValue };
}
