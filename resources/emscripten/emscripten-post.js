Module.initApi = () => {
  Module.api_private.download_js = (pointer, size, filename) => {
    const bytes = HEAPU8.slice(pointer, pointer + size);
    postMessage({type: 'download', filename: UTF8ToString(filename), bytes}, [bytes.buffer]);
  };
  Module.api_private.uploadSavegame_js = slot => postMessage({type: 'upload', kind: 'save', slot});
  Module.api_private.uploadSoundfont_js = () => postMessage({type: 'upload', kind: 'soundfont'});
  Module.api_private.uploadFont_js = () => postMessage({type: 'upload', kind: 'font'});
};
