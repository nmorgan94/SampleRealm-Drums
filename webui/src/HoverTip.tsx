// Mirrors Source/ui/HoverTip.h, plus tap to toggle on touch
import { useEffect, useRef, useState, type PointerEvent as ReactPointerEvent, type ReactNode } from 'react';
import { juceHeight } from './fonts.ts';

// Width comes from the content
type Box = { left: number; top: number; height: number };

interface HoverTipProps {
  text: string;
  target: Box;
  tip: Box;
  children: ReactNode;
}

export function HoverTip({ text, target, tip, children }: HoverTipProps) {
  const targetRef = useRef<HTMLDivElement>(null);
  const [hovered, setHovered] = useState(false);
  const [tapped, setTapped] = useState(false);

  const hover = (shown: boolean) => (e: ReactPointerEvent) => {
    if (e.pointerType !== 'touch') setHovered(shown);
  };

  useEffect(() => {
    if (!tapped) return;
    const dismiss = (e: PointerEvent) => {
      if (!targetRef.current?.contains(e.target as Node)) setTapped(false);
    };
    document.addEventListener('pointerdown', dismiss);
    return () => document.removeEventListener('pointerdown', dismiss);
  }, [tapped]);

  return (
    <>
      <div
        ref={targetRef}
        className="absolute"
        style={target}
        onPointerEnter={hover(true)}
        onPointerLeave={hover(false)}
        onPointerDown={(e) => e.pointerType === 'touch' && setTapped((t) => !t)}
      >
        {children}
      </div>
      {(hovered || tapped) && (
        <div
          className="pointer-events-none absolute flex items-center justify-center border border-line bg-widget px-1"
          style={{ ...tip, borderRadius: 4, fontSize: juceHeight(13) }}
        >
          {text}
        </div>
      )}
    </>
  );
}
