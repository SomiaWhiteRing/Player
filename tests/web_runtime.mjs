import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {test} from 'node:test';
import vm from 'node:vm';

const source = name => readFileSync(new URL(`../resources/emscripten/${name}`, import.meta.url), 'utf8');

test('installed files: small reads, seeks, EOF, block boundaries and bounded eviction', () => {
  class StoredBytes {
    constructor(bytes) { this.bytes = bytes; this.size = bytes.length; }
    slice(start, end) { return new StoredBytes(this.bytes.subarray(start, end)); }
  }
  let physicalReads = 0;
  const context = vm.createContext({self: {}, performance, FileReaderSync: class {
    readAsArrayBuffer(blob) { physicalReads++; return blob.bytes.slice().buffer; }
  }});
  vm.runInContext(source('player-files.js'), context);
  const ops = {};
  const stats = context.self.installPlayerFileCache({lookupPath: () => ({node: {stream_ops: ops}})});
  function file(id, size) {
    const bytes = new Uint8Array(size);
    for (let i = 0; i < size; i++) bytes[i] = (i * 17 + id) % 251;
    return {node: {id, size, contents: new StoredBytes(bytes)}};
  }
  function read(stream, position, length) {
    const bytes = new Uint8Array(length + 14).fill(253);
    const count = ops.read(stream, bytes, 7, length, position);
    assert.equal(count, Math.min(length, Math.max(0, stream.node.size - position)));
    assert.deepEqual(bytes.subarray(7, 7 + count), stream.node.contents.bytes.subarray(position, position + count));
    assert.ok(bytes.subarray(0, 7).every(x => x === 253));
    assert.ok(bytes.subarray(7 + count).every(x => x === 253));
    assert.ok(stats.bytes <= 32 * 1024 * 1024);
  }
  const small = file(1, 417228);
  for (let position = 0; position < small.node.size; position += 2048) read(small, position, 2048);
  assert.equal(physicalReads, 1, 'a whole sound is fetched once instead of hundreds of Blob reads');
  read(small, 19, 3977);
  read(small, small.node.size + 100, 128);
  const large = file(2, 18 * 1024 * 1024 + 199);
  for (const position of [512 * 1024 - 3, 0, 9 * 1024 * 1024 - 22, large.node.size - 19])
    read(large, position, 600123);
  for (let id = 3; id < 26; id++) read(file(id, 2 * 1024 * 1024), 543, 1234);
  const before = physicalReads;
  read(small, 0, 9999);
  assert.equal(physicalReads, before + 1, 'an evicted file is reloaded correctly');
});

test('audio starvation fades to silence and resumes from a refilled direct port', () => {
  let Processor;
  const context = vm.createContext({
    AudioWorkletProcessor: class { constructor() { this.port = {}; } },
    registerProcessor: (_name, value) => { Processor = value; },
  });
  vm.runInContext(source('player-audio.js'), context);
  const requests = [];
  const audioPort = {postMessage: data => requests.push(data)};
  const processor = new Processor();
  processor.port.onmessage({data: {type: 'connect', port: audioPort}});
  processor.port.onmessage({data: {type: 'start'}});
  function process() {
    const output = [new Float32Array(128), new Float32Array(128)];
    assert.equal(processor.process([], [output]), true);
    for (const channel of output) assert.ok(channel.every(Number.isFinite));
    return output;
  }
  process();
  assert.equal(requests.length, 3);
  const supply = () => audioPort.onmessage({data: {pcm: new Int16Array(2048).fill(16384)}});
  supply(); supply();
  for (let i = 0; i < 16; i++) process();
  const faded = process()[0];
  assert.ok(faded[0] > 0.4 && faded[63] === 0);
  assert.ok(faded.every((value, i) => i === 0 || value <= faded[i - 1]));
  supply();
  assert.ok(process()[0].every(value => value === 0), 'wait for enough data before resuming');
  supply();
  const resumed = process()[0];
  assert.ok(resumed[0] < 0.01 && resumed[63] === 0.5);
  assert.ok(resumed.every((value, i) => i === 0 || value >= resumed[i - 1]));
});
