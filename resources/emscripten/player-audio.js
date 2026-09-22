// A bounded queue keeps the audio device independent of rendering frames.
class PlayerAudio extends AudioWorkletProcessor {
  constructor() {
    super();
    this.queue = [];
    this.offset = 0;
    this.pending = 0;
    this.started = false;
    this.port.onmessage = ({data}) => {
      if (data.type === 'start') this.started = true;
      if (data.pcm) { this.pending--; this.queue.push(data.pcm); }
    };
  }
  process(_inputs, outputs) {
    const channels = outputs[0];
    for (let i = 0; i < channels[0].length; i++) {
      const pcm = this.queue[0];
      if (!pcm) break;
      channels[0][i] = pcm[this.offset++] / 32768;
      channels[1][i] = pcm[this.offset++] / 32768;
      if (this.offset === pcm.length) { this.queue.shift(); this.offset = 0; }
    }
    // About 64 ms at 48 kHz, at most three queued or in-flight blocks.
    while (this.started && this.queue.length + this.pending < 3) {
      this.pending++;
      this.port.postMessage({type: 'audio'});
    }
    return true;
  }
}
registerProcessor('easyrpg-audio', PlayerAudio);
