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
  // The loop's patterns, by name: Source/shared/Patterns.h
  patterns: string[];
}

// The audition loop: the current instrument's part of a pattern, by its place in DrumInfo's patterns
export interface Loop {
  pattern: number;
  on: boolean;
}

// The current instrument's bar of the loop
export interface LoopSteps {
  // A velocity each, 1..127. 0 or below rests, and a negative one is the velocity the step comes back at
  steps: number[];
  // The step the loop is on, or -1 while it's off
  playing: number;
}

// Patterns::stepsPerBeat and stepsPerBar, and LoopSteps::onVelocity and toggled
export const STEPS_PER_BEAT = 4;
export const STEPS_PER_BAR = 16;
export const STEP_ON_VELOCITY = 127;
export const toggledStep = (velocity: number) => (velocity === 0 ? STEP_ON_VELOCITY : -velocity);

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

  // Replays the current instrument's last note, at a velocity above 0 and up to 1
  audition(velocity: number): void;
  // Not a parameter, and not saved
  patterns: string[];
  getLoop(): Loop;
  // A new pattern replaces every instrument's steps
  setLoop(loop: Loop): void;
  getLoopSteps(): LoopSteps;
  // velocity as LoopSteps' steps hold it
  setLoopStep(step: number, velocity: number): void;
  // A step changed, a new pattern or instrument, or the loop moving on
  subscribeLoopSteps(listener: (steps: LoopSteps) => void): () => void;
  // Every hit, with the current instrument's last note, the hit's gain against a full one and its velocity, both 0..1
  onHit(listener: (lastNote: number, level: number, velocity: number) => void): () => void;
  lastNote(): number;
  preview(note: number, maxSeconds: number): Promise<Preview>;
}
