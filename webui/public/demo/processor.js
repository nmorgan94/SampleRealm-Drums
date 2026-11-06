const LEVEL_EVERY_BLOCKS = 8;

class EngineProcessor extends AudioWorkletProcessor {
  constructor({ processorOptions: { wasm } }) {
    super();

    try {
      // WASI's clock only seeds juce::Random, which every hit reseeds
      const stub = new Proxy({}, { get: () => () => 0 });
      const instance = new WebAssembly.Instance(new WebAssembly.Module(wasm), new Proxy({}, { get: () => stub }));
      this.engine = instance.exports;
      this.engine._initialize?.();
      this.engine.prepare(sampleRate);
    } catch (error) {
      this.engine = null;
      this.port.postMessage({ type: 'error', message: String(error) });
      return;
    }

    this.blocks = 0;
    this.peak = 0;
    this.hits = this.engine.hitCount();

    this.port.onmessage = ({ data }) => {
      if (data.type === 'param') this.engine.setParam(data.index, data.value);
      else if (data.type === 'midi') this.engine.midi(data.bytes[0], data.bytes[1], data.bytes[2]);
      else if (data.type === 'audition') this.engine.audition();
      else if (data.type === 'envelope') {
        // { time, value, curve } per node, as webui/src/demo/engine.ts writes them
        new Float32Array(this.engine.memory.buffer, this.engine.envelopeNodes(), data.nodes.length).set(data.nodes);
        this.engine.setEnvelope(data.index, data.nodes.length / 3);
      }
    };

    this.port.postMessage({ type: 'ready' });
  }

  process(_inputs, [output]) {
    if (!this.engine) return false;

    const frames = output[0].length;
    const pointer = this.engine.render(frames);

    // Only changes if memory grows
    if (this.view?.buffer !== this.engine.memory.buffer || this.view.byteOffset !== pointer || this.view.length !== frames) {
      this.view = new Float32Array(this.engine.memory.buffer, pointer, frames);
    }

    // The drum is mono, copied to every channel
    for (const channel of output) channel.set(this.view);
    for (const sample of this.view) this.peak = Math.max(this.peak, Math.abs(sample));

    const hits = this.engine.hitCount();
    if (hits !== this.hits) {
      this.hits = hits;
      this.port.postMessage({ type: 'hit', note: this.engine.lastNote() });
    }

    if (++this.blocks % LEVEL_EVERY_BLOCKS === 0) {
      if (this.peak > 0) this.port.postMessage({ type: 'level', peak: this.peak });
      this.peak = 0;
    }

    return true;
  }
}

registerProcessor('engine-processor', EngineProcessor);
