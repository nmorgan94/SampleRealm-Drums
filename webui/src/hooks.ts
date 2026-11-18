import { useEffect, useState } from 'react';
import type { DrumsBackend } from './backend.ts';

// count changes on every hit, from the pad, MIDI or the keyboard; level and velocity are that hit's
export function useHits(backend: DrumsBackend) {
  const [hits, setHits] = useState({ count: 0, level: 1, velocity: 1 });

  useEffect(
    () => backend.onHit((_, level, velocity) => setHits(({ count }) => ({ count: count + 1, level, velocity }))),
    [backend],
  );

  return hits;
}
