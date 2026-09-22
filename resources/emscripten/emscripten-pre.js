// This build runs only in a dedicated Worker owned by the archive site.
Module.arguments = ['--project-path', '/game', '--save-path', `/work-saves/${Module.workId}`,
  ...(Module.gameArguments || [])];
Module.preRun = [].concat(Module.preRun || [], () => {
  if (!Number.isSafeInteger(Module.workId) || Module.workId <= 0)
    throw new Error('Invalid workId');
  FS.mkdir('/game');
  FS.mount(WORKERFS, {packages: Module.gamePackages}, '/game');
  FS.chdir('/game');
  FS.mkdir('/work-saves');
  const savePath = `/work-saves/${Module.workId}`;
  FS.mkdir(savePath);
  FS.mount(IDBFS, {}, savePath);
  FS.mkdir('/home/web_user/.config');
  FS.mount(IDBFS, {}, '/home/web_user/.config');
  addRunDependency('player-storage');
  FS.syncfs(true, error => {
    if (error) { abort(`Could not load player storage: ${error}`); return; }
    removeRunDependency('player-storage');
  });
});

Module.syncSaves = () => {
  // Serialize writes: a later snapshot must not overtake an earlier save.
  Module.savePending = (Module.savePending || Promise.resolve()).catch(() => {}).then(() =>
    new Promise((resolve, reject) => FS.syncfs(false, error => error ? reject(error) : resolve())));
  Module.savePending.catch(error => postMessage({type: 'save-error', message: `存档写入失败：${error}`}));
  return Module.savePending;
};
