type ParamKind = 'real' | 'choice';

// How a real value reads, as Source/params/ParameterFormats.h
export type ParamFormat = 'decibels' | 'hertz' | 'suffix';

export interface ParamInfo {
  id: string;
  name: string;
  kind: ParamKind;
  min: number;
  max: number;
  defaultValue: number;
  step: number;
  skew: number;
  format: ParamFormat;
  suffix: string;
  decimals: number;
  automatable: boolean;
  choices: string[];
}

// Display units: seconds, then Hz or gain; curve is the tension of the segment into the node, -1..1
export interface EnvelopeNode {
  time: number;
  value: number;
  curve: number;
}

// Source/shared/DrumEnvelopes.h's EnvelopeSpec
export interface EnvelopeInfo {
  id: string;
  scale: 'linear' | 'logarithmic';
  min: number;
  max: number;
  maxTime: number;
  nodes: EnvelopeNode[];
}

export interface UiInfo {
  version: string;
  minusInfinityDb: number;
  params: ParamInfo[];
  envelopes: EnvelopeInfo[];
}

// Linear peak per channel since the last level
export interface Level {
  peaks: number[];
}

// One hit, for the waveform behind the envelopes
export interface Preview {
  samples: Float32Array;
  sampleRate: number;
  // Where the pitch settles; 0 for the cymbal
  tailHz: number;
}

export interface PresetEntry {
  name: string;
  category: string;
  isFactory: boolean;
}

export interface PresetStatus {
  current: PresetEntry;
  modified: boolean;
}

// Mirrors Source/presets/PresetManager.h
export interface PresetBackend {
  status(): PresetStatus;
  onStatus(listener: (status: PresetStatus) => void): () => void;
  // Factory then user presets of the current instrument, rescanning the user ones
  list(): Promise<PresetEntry[]>;
  load(preset: PresetEntry): Promise<void>;
  loadInit(): Promise<void>;
  loadNext(): Promise<void>;
  loadPrevious(): Promise<void>;
  exists(name: string): Promise<boolean>;
  // Each resolves to an error message, or null
  save(name: string, category: string): Promise<string | null>;
  rename(preset: PresetEntry, newName: string): Promise<string | null>;
  remove(preset: PresetEntry): Promise<string | null>;
  // Only where presets are files
  showFolder?(): void;
}

export const isSamePreset = (a: PresetEntry, b: PresetEntry) => a.name === b.name && a.isFactory === b.isFactory;

export interface DrumsBackend {
  version: string;
  presets: PresetBackend;
  param(id: string): ParamInfo;
  // Real values, not 0..1
  getValue(id: string): number;
  setValue(id: string, value: number): void;
  beginGesture(id: string): void;
  endGesture(id: string): void;
  subscribe(id: string, listener: (value: number) => void): () => void;
  onLevel(listener: (level: Level) => void): () => void;

  // By DrumEnvelopes::Index
  envelopes: EnvelopeInfo[];
  getEnvelope(index: number): EnvelopeNode[];
  // A user edit, which marks the preset modified
  setEnvelope(index: number, nodes: EnvelopeNode[]): void;
  subscribeEnvelope(index: number, listener: (nodes: EnvelopeNode[]) => void): () => void;

  // Replays the current instrument's last note
  audition(): void;
  // Every hit, with the current instrument's last note
  onHit(listener: (lastNote: number) => void): () => void;
  lastNote(): number;
  preview(note: number, maxSeconds: number): Promise<Preview>;
}

export const paramLookup = <T>(entries: Map<string, T>) => (id: string) => {
  const found = entries.get(id);
  if (!found) throw new Error(`No parameter ${id}`);
  return found;
};
