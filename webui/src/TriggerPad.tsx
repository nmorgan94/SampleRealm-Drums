// Mirrors Source/ui/TriggerPad.h
import { useEffect, useRef, useState } from 'react';
import { juceHeight } from './fonts.ts';
import type { Box } from './layout.ts';

// TriggerPad::timerCallback: at 60 Hz, by 0.85 a tick until under 0.01
const decayed = (brightness: number, elapsedMs: number) => {
  const next = brightness * 0.85 ** (elapsedMs / (1000 / 60));
  return next < 0.01 ? 0 : next;
};

// Fires on press and flashes whenever hits changes, including hits from MIDI
export function TriggerPad({ text, box, hits, onTrigger }: { text: string; box: Box; hits: number; onTrigger(): void }) {
  const [brightness, setBrightness] = useState(0);
  const frame = useRef(0);

  useEffect(() => {
    if (hits === 0) return;

    const start = performance.now();
    const tick = (now: number) => {
      const next = decayed(1, now - start);
      setBrightness(next);
      if (next > 0) frame.current = requestAnimationFrame(tick);
    };

    setBrightness(1);
    frame.current = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(frame.current);
  }, [hits]);

  return (
    <button
      type="button"
      className="absolute cursor-pointer rounded-md font-bold text-fg"
      style={{
        ...box,
        fontSize: juceHeight(18),
        // Colour::interpolatedWith the accent, and the outline's alpha
        background: `color-mix(in srgb, var(--color-accent) ${brightness * 80}%, var(--color-widget))`,
        boxShadow: `inset 0 0 0 1.5px color-mix(in srgb, var(--color-accent) ${(0.4 + 0.6 * brightness) * 100}%, transparent)`,
      }}
      onPointerDown={(e) => e.button === 0 && onTrigger()}
    >
      {text}
    </button>
  );
}
