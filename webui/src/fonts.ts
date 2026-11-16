import { juceHeight } from '@samplerealm/modules/editor/fonts.ts';

let canvas: HTMLCanvasElement | undefined;

// juce::GlyphArrangement::getStringWidth in Orbitron at a JUCE height
export function textWidth(text: string, height: number, bold = false) {
  canvas ??= typeof document === 'undefined' ? undefined : document.createElement('canvas');
  const context = canvas?.getContext('2d');
  if (!context) return text.length * height * 0.8;
  context.font = `${bold ? 700 : 400} ${juceHeight(height)}px Orbitron`;
  return context.measureText(text).width;
}
