import { useEffect, useState } from 'react';
import type { DrumsBackend } from './backend.ts';

// Changes on every hit, from the pad, MIDI or the keyboard
export function useHits(backend: DrumsBackend) {
  const [hits, setHits] = useState(0);

  useEffect(() => backend.onHit(() => setHits((count) => count + 1)), [backend]);

  return hits;
}
