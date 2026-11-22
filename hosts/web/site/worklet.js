class BlueProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.api = new WebAssembly.Instance(
      options.processorOptions.module,
      {},
    ).exports;
    this.api._initialize();
    if (!this.api.audio_init(sampleRate))
      throw Error("Unsupported sample rate");
    this.heap = new Uint8Array(this.api.memory.buffer);
    this.l = new Float32Array(this.heap.buffer, this.api.audio_left(), 2048);
    this.r = new Float32Array(this.heap.buffer, this.api.audio_right(), 2048);
    this.port.onmessage = ({ data }) => {
      if (data.buffer) {
        if (data.buffer.byteLength !== this.api.audio_size()) return;
        this.pending = {
          bytes: new Uint8Array(data.buffer),
          offset: 0,
          ptr: 0,
          id: data.id,
        };
      } else this.api.audio_event(data.c, data.i, data.v);
    };
  }
  process(inputs, outputs) {
    const out = outputs[0],
      patch = this.pending;
    if (patch) {
      if (!patch.ptr) patch.ptr = this.api.audio_stage();
      if (patch.ptr) {
        const end = Math.min(patch.offset + 8192, patch.bytes.length);
        this.heap.set(
          patch.bytes.subarray(patch.offset, end),
          patch.ptr + patch.offset,
        );
        patch.offset = end;
        if (end === patch.bytes.length) {
          this.api.audio_commit();
          this.pending = null;
          this.port.postMessage({ applied: patch.id });
        }
      }
    }
    for (let start = 0; start < out[0].length; start += 2048) {
      const count = Math.min(2048, out[0].length - start);
      this.api.audio_render(count);
      for (let i = 0; i < count; i++) {
        out[0][start + i] = this.l[i];
        out[1][start + i] = this.r[i];
      }
    }
    return true;
  }
}
registerProcessor("blue-render", BlueProcessor);
class BlueRecorder extends AudioWorkletProcessor {
  constructor() {
    super();
    this.buffer = null;
    this.offset = 0;
    this.port.onmessage = ({ data }) => {
      if (data) {
        this.buffer = new Float32Array(sampleRate * 2 * 60);
        this.offset = 0;
      } else if (this.buffer) {
        const samples = this.buffer.slice(0, this.offset);
        this.buffer = null;
        this.port.postMessage({ samples, rate: sampleRate }, [samples.buffer]);
      }
    };
  }
  process(inputs, outputs) {
    const [l, r] = inputs[0];
    if (!l) return true;
    outputs[0][0].set(l);
    outputs[0][1].set(r || l);
    if (this.buffer)
      for (let i = 0; i < l.length && this.offset < this.buffer.length; i++) {
        this.buffer[this.offset++] = l[i];
        this.buffer[this.offset++] = (r || l)[i];
      }
    return true;
  }
}
registerProcessor("blue-recorder", BlueRecorder);
