// Mirrors Source/ui/TriggerPad.h
import { useEffect, useRef, useState } from 'react';
import { juceHeight } from '@samplerealm/modules/editor/fonts.ts';
import type { Rect } from '@samplerealm/modules/editor/layout.ts';
import { clamp01 } from '@samplerealm/modules/editor/params.ts';
import { clamp } from './envelope.ts';

// TriggerPad::timerCallback: at 60 Hz, by 0.85 a tick until under 0.01
const decayed = (brightness: number, elapsedMs: number) => {
  const next = brightness * 0.85 ** (elapsedMs / (1000 / 60));
  return next < 0.01 ? 0 : next;
};

// TriggerPad::minFlash, velocityInset, barWidth, numberHeight, tickHeight and tickOverhang
const MIN_FLASH = 0.25;
const TICK_HEIGHT = 1.5;
const TICK_OVERHANG = 2;
const VELOCITY_INSET = 8;
const BAR_WIDTH = 3;
const NUMBER_HEIGHT = 11;

// TriggerPad::velocityAt
const velocityAt = (y: number, height: number) => clamp((height - VELOCITY_INSET - y) / (height - 2 * VELOCITY_INSET), 1 / 127, 1);

interface TriggerPadProps {
  text: string;
  box: Rect;
  // Flashes whenever count changes, including hits from MIDI; a softer hit flashes dimmer
  hits: { count: number; level: number; velocity: number };
  // Harder the higher the pad is pressed
  onTrigger(velocity: number): void;
}

export function TriggerPad({ text, box, hits, onTrigger }: TriggerPadProps) {
  const [brightness, setBrightness] = useState(0);
  const frame = useRef(0);

  useEffect(() => {
    if (hits.count === 0) return;

    const start = performance.now();
    const flash = clamp(hits.level, MIN_FLASH, 1);
    const tick = (now: number) => {
      const next = decayed(flash, now - start);
      setBrightness(next);
      if (next > 0) frame.current = requestAnimationFrame(tick);
    };

    setBrightness(flash);
    frame.current = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(frame.current);
  }, [hits]);

  const velocity = clamp01(hits.velocity);
  const level = clamp01(hits.level);

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
      onPointerDown={(e) => {
        if (e.button !== 0) return;
        // The editor can be scaled, so the press is measured against the pad as drawn
        const drawn = e.currentTarget.getBoundingClientRect();
        onTrigger(velocityAt(((e.clientY - drawn.top) / drawn.height) * box.height, box.height));
      }}
    >
      {text}
      {hits.count > 0 && (
        <>
          <span
            className="pointer-events-none absolute rounded-full"
            style={{
              left: VELOCITY_INSET,
              top: VELOCITY_INSET,
              bottom: VELOCITY_INSET,
              width: BAR_WIDTH,
              background: 'color-mix(in srgb, var(--color-accent) 25%, transparent)',
            }}
          >
            <span className="absolute inset-x-0 bottom-0 rounded-full bg-accent" style={{ height: `${level * 100}%` }} />
            <span
              className="absolute bg-fg"
              style={{ left: -TICK_OVERHANG, right: -TICK_OVERHANG, top: `${(1 - velocity) * 100}%`, height: TICK_HEIGHT }}
            />
          </span>
          <span
            className="pointer-events-none absolute inset-x-0 text-center opacity-70"
            style={{ bottom: VELOCITY_INSET, fontSize: juceHeight(NUMBER_HEIGHT), lineHeight: `${NUMBER_HEIGHT}px` }}
          >
            {Math.round(velocity * 127)}
          </span>
        </>
      )}
    </button>
  );
}
