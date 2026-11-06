// Mirrors Source/ui/LevelMeter.h: one bar, as the drum is mono
import { useEffect, useRef, useState } from 'react';
import type { DrumsBackend } from './backend.ts';
import type { Box } from './layout.ts';
import { barChanged, emptyBar, FLOOR_DB, isClipping, updateBar, UPDATE_HZ } from './meter.ts';

function useBar(backend: DrumsBackend) {
  const [bar, setBar] = useState(emptyBar);
  // The loudest peak since the last tick
  const pending = useRef(0);

  useEffect(() => backend.onLevel(({ peaks }) => (pending.current = Math.max(pending.current, ...peaks))), [backend]);

  useEffect(() => {
    const timer = window.setInterval(() => {
      const peak = pending.current;
      pending.current = 0;
      setBar((current) => {
        const next = updateBar(current, peak);
        return barChanged(current, next) ? next : current;
      });
    }, 1000 / UPDATE_HZ);
    return () => window.clearInterval(timer);
  }, []);

  return bar;
}

export function LevelMeter({ backend, box }: { backend: DrumsBackend; box: Box }) {
  const bar = useBar(backend);
  const width = box.width - 1;
  const top = 0.5 + 6 + 3;
  const bodyHeight = box.height - 1 - 6 - 3;
  const fillHeight = (bodyHeight - 2) * ((bar.levelDb - FLOOR_DB) / -FLOOR_DB);

  return (
    <svg className="absolute" style={box} width={box.width} height={box.height} aria-hidden>
      <rect x={0.5} y={0.5} width={width} height={6} rx={2} fill={isClipping(bar) ? 'var(--color-clip)' : 'var(--color-line)'} />
      <rect x={0.5} y={top} width={width} height={bodyHeight} rx={2} fill="var(--color-panel)" />
      {fillHeight > 0 && <rect x={1.5} y={top + 1 + bodyHeight - 2 - fillHeight} width={width - 2} height={fillHeight} rx={1.5} fill="var(--color-accent-alt)" />}
      <rect x={0.5} y={top} width={width} height={bodyHeight} rx={2} fill="none" stroke="var(--color-line)" strokeWidth={1} />
    </svg>
  );
}
