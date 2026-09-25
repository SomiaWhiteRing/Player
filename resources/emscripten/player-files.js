// WORKERFS files are immutable slices of installed OPFS packs. Amortize the
// browser's per-read cost without copying the entire game into WASM memory.
self.installPlayerFileCache = function installPlayerFileCache(FS, budget = 32 * 1024 * 1024) {
  const ops = FS.lookupPath('/game').node.stream_ops;
  const entries = new Map();
  const reader = new FileReaderSync();
  const wholeFileLimit = 16 * 1024 * 1024;
  const blockSize = 512 * 1024;
  let bytes = 0;
  const stats = {hits: 0, misses: 0, bytes: 0, peakBytes: 0, readMs: 0};
  ops.read = (stream, buffer, offset, length, position) => {
    const node = stream.node;
    const count = Math.min(length, Math.max(0, node.size - position));
    let copied = 0;
    while (copied < count) {
      const start = node.size <= wholeFileLimit ? 0 : Math.floor((position + copied) / blockSize) * blockSize;
      const key = `${node.id}:${start}`;
      let value = entries.get(key);
      if (value) {
        stats.hits++;
        entries.delete(key);
      } else {
        stats.misses++;
        const end = Math.min(node.size, start + (node.size <= wholeFileLimit ? wholeFileLimit : blockSize));
        while (bytes + end - start > budget && entries.size) {
          const oldest = entries.keys().next().value;
          bytes -= entries.get(oldest).length;
          entries.delete(oldest);
        }
        const t = performance.now();
        value = new Uint8Array(reader.readAsArrayBuffer(node.contents.slice(start, end)));
        stats.readMs += performance.now() - t;
        bytes += value.length;
      }
      entries.set(key, value);
      const from = position + copied - start;
      const size = Math.min(count - copied, value.length - from);
      buffer.set(value.subarray(from, from + size), offset + copied);
      copied += size;
    }
    stats.bytes = bytes;
    stats.peakBytes = Math.max(stats.peakBytes, bytes);
    return count;
  };
  return stats;
};
