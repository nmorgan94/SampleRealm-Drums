import type { CSSProperties, ReactNode } from 'react';
import { juceHeight } from './fonts.ts';

// Literal class names, so Tailwind finds them
const COLOURS = { dim: 'text-dim', accent: 'text-accent' };

interface TextProps {
  style: CSSProperties;
  // The JUCE font height
  size: number;
  bold?: boolean;
  colour?: keyof typeof COLOURS;
  right?: boolean;
  bottom?: boolean;
  children: ReactNode;
}

// One line of text in a box, centred vertically unless bottom
export function Text({ style, size, bold = false, colour, right = false, bottom = false, children }: TextProps) {
  const className = ['absolute flex', bottom ? 'items-end' : 'items-center', right && 'justify-end', bold && 'font-bold', colour && COLOURS[colour]];

  return (
    <div className={className.filter(Boolean).join(' ')} style={{ ...style, fontSize: juceHeight(size) }}>
      {children}
    </div>
  );
}
