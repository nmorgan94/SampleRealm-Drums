// JUCE font heights are ascent + descent, CSS sizes are the em: Orbitron's is 1254 / 1000
export const juceHeight = (height: number) => (height * 1000) / 1254;

let canvas: HTMLCanvasElement | undefined;

// juce::GlyphArrangement::getStringWidth in Orbitron at a JUCE height
export function textWidth(text: string, height: number, bold = false) {
  canvas ??= typeof document === 'undefined' ? undefined : document.createElement('canvas');
  const context = canvas?.getContext('2d');
  if (!context) return text.length * height * 0.8;
  context.font = `${bold ? 700 : 400} ${juceHeight(height)}px Orbitron`;
  return context.measureText(text).width;
}
