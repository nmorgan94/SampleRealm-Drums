import type { ParamInfo, PluginBackend } from '@samplerealm/modules/backend.ts';

export * from '@samplerealm/modules/backend.ts';

// How a real value reads, as Parameters::withValueText
export interface ValueText {
  format: 'decibels' | 'hertz' | 'suffix';
  suffix: string;
  decimals: number;
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

// Source/shared/DrumInfoJson.h
export interface DrumInfo {
  minusInfinityDb: number;
  // By the parameter's place in the ui info
  text: ValueText[];
  envelopes: EnvelopeInfo[];
}

// One hit, for the waveform behind the envelopes
export interface Preview {
  samples: Float32Array;
  sampleRate: number;
  // Where the pitch settles; 0 for the cymbal
  tailHz: number;
}

export interface DrumsBackend extends PluginBackend {
  // As the plugin shows a value and reads typed text
  formatValue(param: ParamInfo, value: number): string;
  parseValue(param: ParamInfo, text: string): number | null;

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
