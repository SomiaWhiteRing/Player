package org.easyrpg.player.imports;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.text.format.Formatter;
import android.util.AtomicFile;
import android.util.Log;

import androidx.documentfile.provider.DocumentFile;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import org.easyrpg.player.Helper;
import org.easyrpg.player.R;
import org.easyrpg.player.imports.WebImportClient.Metadata;
import org.easyrpg.player.imports.WebImportClient.Stage;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Application-owned queue. Activities only observe it; partial ZIPs survive process death. */
public final class WebImportDownloads {
    private static WebImportDownloads instance;
    private final Application context;
    private final SharedPreferences journal;
    private final File directory;
    private final LinkedHashMap<String, Task> tasks = new LinkedHashMap<>();
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final ExecutorService journalWriter = Executors.newSingleThreadExecutor();
    private final MutableLiveData<List<Download>> updates = new MutableLiveData<>();

    public static synchronized WebImportDownloads get(Context context) {
        if (instance == null) instance = new WebImportDownloads(context.getApplicationContext());
        return instance;
    }

    public static final class Download {
        public final String id;
        public final Metadata metadata;
        public final Bitmap cover;
        public final Stage stage;
        public final int percent, message;
        public final long bytes, speed;

        Download(Task task) {
            id = task.id;
            metadata = task.metadata;
            cover = task.cover;
            stage = task.stage;
            // This is download progress, so later verification/copy percentages must not reset it.
            percent = (int) Math.min(100, task.bytes * 100 / metadata.zipSize);
            message = preparing() ? R.string.web_import_preparing : task.message;
            bytes = task.bytes;
            speed = task.speed;
        }

        public boolean active() { return isActive(stage); }
        public boolean resumable() { return stage == Stage.PAUSED || stage == Stage.ERROR; }
        public boolean preparing() { return stage == Stage.VERIFYING || stage == Stage.SAVING; }
        public boolean indeterminate() { return preparing() || stage == Stage.QUEUED || stage == Stage.PAUSING || stage == Stage.REMOVING; }

        public String transferText(Context context) {
            String completed = Formatter.formatFileSize(context, bytes);
            String total = Formatter.formatFileSize(context, metadata.zipSize);
            return stage == Stage.DOWNLOADING
                    ? context.getString(R.string.web_import_transfer, percent, completed, total, Formatter.formatFileSize(context, speed))
                    : context.getString(R.string.web_import_transfer_paused, percent, completed, total);
        }
    }

    private static final class Task {
        String id, pending;
        Uri source, folder;
        Metadata metadata;
        Bitmap cover;
        Stage stage;
        int percent, message;
        long bytes, speed;
        boolean scheduled, pauseRequested;
        WebImportClient client;
    }

    private WebImportDownloads(Context context) {
        this.context = (Application) context;
        journal = context.getSharedPreferences("web-import-downloads", Context.MODE_PRIVATE);
        // Cache files can be evicted by Android. Resumable downloads belong in private files instead.
        directory = new File(context.getNoBackupFilesDir(), "web-import");
        try {
            JSONArray saved = new JSONArray(journal.getString("tasks", "[]"));
            for (int i = 0; i < saved.length(); i++) {
                JSONObject item = saved.getJSONObject(i);
                Task task = new Task();
                task.id = UUID.fromString(item.getString("id")).toString();
                task.source = Uri.parse(item.getString("source"));
                task.folder = Uri.parse(item.getString("folder"));
                JSONObject meta = item.getJSONObject("metadata");
                String cover = meta.optString("coverUrl", "");
                task.metadata = new Metadata(meta.getString("title"), meta.getLong("archiveVersionId"),
                        meta.getString("manifestSha256"), Uri.parse(meta.getString("downloadUrl")),
                        meta.getLong("zipSizeBytes"), Collections.emptyMap(), meta.optLong("workId"),
                        cover.isEmpty() || cover.equals("null") ? null : Uri.parse(cover), meta.toString());
                task.stage = Stage.valueOf(item.getString("stage"));
                if (task.stage == Stage.PAUSING || task.stage == Stage.REMOVING) task.stage = Stage.PAUSED;
                else if (isActive(task.stage)) task.stage = Stage.QUEUED;
                task.message = task.stage == Stage.COMPLETE ? R.string.web_import_complete :
                        task.stage == Stage.ERROR ? R.string.web_import_failed :
                        task.stage == Stage.QUEUED ? R.string.web_import_queued : R.string.web_import_paused;
                task.pending = item.optString("pending", null);
                task.bytes = zipFile(task).length();
                task.percent = (int) Math.min(100, task.bytes * 100 / task.metadata.zipSize);
                tasks.put(task.id, task);
            }
        } catch (Exception e) {
            Log.w("KaiImport", "Cannot restore download queue", e);
        }
        publish();
        worker.execute(() -> {
            synchronized (this) {
                for (Task task : tasks.values()) {
                    if (task.stage != Stage.COMPLETE) task.cover = BitmapFactory.decodeFile(coverFile(task).getPath());
                }
                publish();
            }
        });
    }

    public LiveData<List<Download>> observe() { return updates; }

    public synchronized List<Download> snapshot() {
        List<Download> result = new ArrayList<>();
        for (Task task : tasks.values()) result.add(new Download(task));
        return result;
    }

    private void publish() { updates.postValue(snapshot()); }

    private static boolean isActive(Stage stage) {
        return stage == Stage.QUEUED || stage == Stage.DOWNLOADING || stage == Stage.VERIFYING ||
                stage == Stage.SAVING || stage == Stage.PAUSING;
    }

    public synchronized boolean hasActive() {
        for (Task task : tasks.values()) if (isActive(task.stage)) return true;
        return false;
    }

    public synchronized boolean hasPending() {
        for (Task task : tasks.values()) if (task.stage != Stage.COMPLETE) return true;
        return false;
    }

    private static boolean sameGame(Metadata left, Metadata right) {
        return left.fileName.equals(right.fileName) ||
                (left.workId > 0 && left.workId == right.workId &&
                        left.download.getHost().equals(right.download.getHost()));
    }

    public synchronized boolean contains(Metadata metadata, Uri folder) {
        for (Task task : tasks.values()) {
            if (task.stage != Stage.COMPLETE && task.folder.equals(folder) && sameGame(metadata, task.metadata)) return true;
        }
        return false;
    }

    /** Uses actual files, never just a stale installation record. Call off the UI thread. */
    public boolean isInstalled(Metadata metadata, DocumentFile games) {
        if (games == null || !games.isDirectory() || !games.canRead()) return false;
        Set<String> names = new HashSet<>();
        for (String[] child : Helper.listChildrenDocuments(context, games.getUri())) names.add(child[2]);
        String prefix = metadata.fileName.substring(0, metadata.fileName.lastIndexOf('-') + 1);
        for (String name : names) {
            if (name.equals(metadata.fileName) || (name.startsWith(prefix) && name.endsWith(".zip"))) return true;
        }
        synchronized (this) {
            for (Task task : tasks.values()) {
                if (task.folder.equals(games.getUri()) && sameGame(metadata, task.metadata) && names.contains(task.metadata.fileName)) return true;
            }
        }
        return false;
    }

    public synchronized void enqueue(Uri link, Metadata metadata, Bitmap cover, Uri folder) throws IOException {
        if (contains(metadata, folder)) return;
        Task task = new Task();
        task.id = UUID.randomUUID().toString();
        task.source = link;
        task.metadata = metadata;
        task.cover = cover;
        task.folder = folder;
        task.stage = Stage.QUEUED;
        task.message = R.string.web_import_queued;
        // Persist every queued manifest before accepting the import, including tasks waiting behind another download.
        if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Cannot create download directory");
        AtomicFile file = metadataFile(task);
        FileOutputStream out = file.startWrite();
        try {
            out.write(metadata.json.getBytes(StandardCharsets.UTF_8));
            file.finishWrite(out);
        } catch (IOException e) { file.failWrite(out); throw e; }
        if (cover != null) {
            try (FileOutputStream image = new FileOutputStream(coverFile(task))) { cover.compress(Bitmap.CompressFormat.PNG, 100, image); }
        }
        tasks.put(task.id, task);
        try { persist(); }
        catch (IOException e) { tasks.remove(task.id); file.delete(); throw e; }
        publish();
    }

    public synchronized void startQueued() {
        for (Task task : tasks.values()) {
            if (task.stage == Stage.QUEUED && !task.scheduled) {
                task.scheduled = true;
                task.pauseRequested = false;
                worker.execute(() -> run(task));
            }
        }
    }

    public synchronized void pause(String id) {
        Task task = tasks.get(id);
        if (task == null || !isActive(task.stage)) return;
        task.pauseRequested = true;
        task.speed = 0;
        if (task.client != null) {
            task.stage = Stage.PAUSING;
            task.message = R.string.web_import_pausing;
            task.client.cancel();
        } else {
            task.stage = Stage.PAUSED;
            task.message = R.string.web_import_paused;
        }
        publish();
        journalWriter.execute(this::persistQuietly);
    }

    public synchronized void pauseAll() {
        for (Task task : tasks.values()) pause(task.id);
    }

    public synchronized void resume(String id) {
        Task task = tasks.get(id);
        if (task == null || !(task.stage == Stage.PAUSED || task.stage == Stage.ERROR)) return;
        task.pauseRequested = false;
        task.stage = Stage.QUEUED;
        task.message = R.string.web_import_queued;
        publish();
        journalWriter.execute(this::persistQuietly);
    }

    public synchronized void startFailed() {
        for (Task task : tasks.values()) {
            if (task.stage == Stage.QUEUED && task.client == null) {
                task.stage = Stage.ERROR;
                task.message = R.string.web_import_background_failed;
            }
        }
        publish();
        journalWriter.execute(this::persistQuietly);
    }

    /** Removal is offered only after the worker has stopped; installed games are never deleted. */
    public synchronized void remove(String id) {
        Task task = tasks.get(id);
        if (task == null || !(task.stage == Stage.PAUSED || task.stage == Stage.ERROR) || task.client != null) return;
        task.stage = Stage.REMOVING;
        task.message = R.string.web_import_removing;
        publish();
        journalWriter.execute(this::persistQuietly);
        worker.execute(() -> {
            try {
                DocumentFile games = Helper.getFileFromURI(context, task.folder);
                if (games != null) cleanupPending(task, games);
                if (zipFile(task).exists() && !zipFile(task).delete()) throw new IOException("Cannot delete partial download");
                metadataFile(task).delete();
                if (coverFile(task).exists() && !coverFile(task).delete()) Log.w("KaiImport", "Cannot remove private cover");
                synchronized (this) {
                    tasks.remove(task.id);
                    persist();
                    publish();
                }
            } catch (Exception e) {
                Log.w("KaiImport", "Cannot remove download", e);
                synchronized (this) {
                    tasks.put(task.id, task);
                    task.stage = Stage.ERROR;
                    task.message = R.string.web_import_storage_error;
                    publish();
                }
            }
        });
    }

    private File zipFile(Task task) { return new File(directory, task.id + ".zip"); }
    private File coverFile(Task task) { return new File(directory, task.id + ".png"); }
    private AtomicFile metadataFile(Task task) { return new AtomicFile(new File(directory, task.id + ".json")); }

    private void run(Task task) {
        try {
            AtomicFile file = metadataFile(task);
            if (task.metadata.files.isEmpty()) {
                Metadata metadata = WebImportClient.parseMetadata(WebImportClient.parseLink(task.source),
                        new String(file.readFully(), StandardCharsets.UTF_8));
                synchronized (this) { task.metadata = metadata; }
            }
            persist();
            synchronized (this) {
                if (task.pauseRequested || task.stage != Stage.QUEUED) return;
                task.client = new WebImportClient(context, new WebImportClient.Progress() {
                    @Override public void update(Stage stage, int percent, long bytes, long speed, int message) {
                        synchronized (WebImportDownloads.this) {
                            if (task.pauseRequested) return;
                            boolean changed = task.stage != stage;
                            task.stage = stage;
                            task.percent = percent;
                            task.bytes = bytes;
                            task.speed = speed;
                            task.message = message;
                            publish();
                            if (changed) persistQuietly();
                        }
                    }

                    @Override public void pending(Uri uri) throws IOException {
                        synchronized (WebImportDownloads.this) {
                            task.pending = uri == null ? null : uri.toString();
                            persist();
                        }
                    }
                });
            }
            DocumentFile games = Helper.getFileFromURI(context, task.folder);
            if (games == null || !games.isDirectory() || !games.canRead() || !games.canWrite()) {
                throw new WebImportClient.ImportException(R.string.web_import_storage_error);
            }
            cleanupPending(task, games);
            if (!isInstalled(task.metadata, games)) {
                task.client.install(task.metadata, zipFile(task), games, ".kai-import-" + task.id + ".part");
            }
            synchronized (this) {
                task.stage = Stage.COMPLETE;
                task.percent = 100;
                task.bytes = task.metadata.zipSize;
                task.message = R.string.web_import_complete;
                persist();
            }
            // A completed installation record remains for work-ID-based duplicate detection.
            if (zipFile(task).exists() && !zipFile(task).delete()) Log.w("KaiImport", "Cannot remove completed private ZIP");
            file.delete();
            if (coverFile(task).exists() && !coverFile(task).delete()) Log.w("KaiImport", "Cannot remove completed private cover");
            synchronized (this) {
                Metadata source = task.metadata;
                task.metadata = new Metadata(source.title, source.archiveVersionId, source.manifestSha256,
                        source.download, source.zipSize, Collections.emptyMap(), source.workId, source.coverUrl, "{}");
                task.cover = null;
            }
        } catch (Exception e) {
            Log.w("KaiImport", "Download stopped", e);
            synchronized (this) {
                task.stage = task.pauseRequested ? Stage.PAUSED : Stage.ERROR;
                task.bytes = zipFile(task).length();
                task.percent = (int) Math.min(100, task.bytes * 100 / task.metadata.zipSize);
                task.message = task.pauseRequested ? R.string.web_import_paused :
                        e instanceof WebImportClient.ImportException ? ((WebImportClient.ImportException) e).message : R.string.web_import_failed;
                if (task.message == R.string.web_import_integrity_error && zipFile(task).exists()) {
                    if (zipFile(task).delete()) { task.bytes = 0; task.percent = 0; }
                }
                persistQuietly();
            }
        } finally {
            synchronized (this) {
                task.client = null;
                task.scheduled = false;
                task.speed = 0;
                if (task.stage == Stage.PAUSING) { task.stage = Stage.PAUSED; task.message = R.string.web_import_paused; }
                persistQuietly();
                publish();
                if (task.stage == Stage.QUEUED) startQueued();
            }
        }
    }

    private void cleanupPending(Task task, DocumentFile games) throws IOException {
        DocumentFile pending = task.pending == null ? games.findFile(".kai-import-" + task.id + ".part") :
                DocumentFile.fromSingleUri(context, Uri.parse(task.pending));
        if (pending != null && pending.exists()) {
            String name = pending.getName();
            if (name != null && name.startsWith(".kai-import-") && name.endsWith(".part") && !pending.delete()) {
                throw new WebImportClient.ImportException(R.string.web_import_storage_error);
            }
        }
        synchronized (this) {
            task.pending = null;
            persist();
        }
    }

    private synchronized void persist() throws IOException {
        try {
            JSONArray saved = new JSONArray();
            for (Task task : tasks.values()) {
                Metadata source = task.metadata;
                JSONObject metadata = new JSONObject().put("title", source.title).put("workId", source.workId)
                        .put("archiveVersionId", source.archiveVersionId).put("manifestSha256", source.manifestSha256)
                        .put("downloadUrl", source.download.toString()).put("zipSizeBytes", source.zipSize)
                        .put("coverUrl", source.coverUrl == null ? null : source.coverUrl.toString());
                saved.put(new JSONObject().put("id", task.id).put("source", task.source.toString())
                        .put("folder", task.folder.toString()).put("metadata", metadata)
                        .put("stage", task.stage.name()).put("pending", task.pending));
            }
            if (!journal.edit().putString("tasks", saved.toString()).commit()) throw new IOException("Cannot save download queue");
        } catch (org.json.JSONException e) { throw new IOException(e); }
    }

    private void persistQuietly() {
        try { persist(); } catch (IOException e) { Log.w("KaiImport", "Cannot save queue", e); }
    }
}
