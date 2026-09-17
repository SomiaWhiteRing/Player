package org.easyrpg.player.imports;

import android.app.Application;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.documentfile.provider.DocumentFile;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.MutableLiveData;

import org.easyrpg.player.Helper;
import org.easyrpg.player.R;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/** Downloads only public archive snapshots from the two explicitly trusted site origins. */
public class WebImportViewModel extends AndroidViewModel {
    private static final long MAX_ZIP = 1024L * 1024 * 1024;
    private static final int MAX_METADATA = 8 * 1024 * 1024;
    private static final int MAX_FILES = 50000;
    // Also serializes cleanup with a previous activity whose cancellation is still finishing.
    private static final ExecutorService EXECUTOR = Executors.newSingleThreadExecutor();
    private final AtomicBoolean cancelled = new AtomicBoolean();
    private volatile HttpURLConnection connection;
    private Uri source;
    private Metadata metadata;
    public final MutableLiveData<State> state = new MutableLiveData<>();

    public enum Stage { LOADING, READY, DOWNLOADING, VERIFYING, SAVING, COMPLETE, ERROR, CANCELLED }

    public static final class State {
        public final Stage stage;
        public final Metadata metadata;
        public final int progress;
        public final int message;
        State(Stage stage, Metadata metadata, int progress, int message) {
            this.stage = stage;
            this.metadata = metadata;
            this.progress = progress;
            this.message = message;
        }
        public boolean busy() {
            return stage == Stage.LOADING || stage == Stage.DOWNLOADING ||
                    stage == Stage.VERIFYING || stage == Stage.SAVING;
        }
    }

    public static final class Metadata {
        public final String title, manifestSha256, fileName;
        public final Uri download;
        public final long archiveVersionId, zipSize;
        final Map<String, Entry> files;
        Metadata(String title, long id, String hash, Uri download, long zipSize, Map<String, Entry> files) {
            this.title = title;
            this.archiveVersionId = id;
            this.manifestSha256 = hash;
            this.download = download;
            this.zipSize = zipSize;
            this.files = files;
            // Stable per site + snapshot; staging IDs must never collide with production IDs.
            String site = download.getHost().contains("-staging.") ? "staging-" : "";
            this.fileName = "VIPRPG-" + site + id + "-" + hash.substring(0, 16) + ".zip";
        }
    }

    private static final class Entry {
        final long size;
        final String hash;
        Entry(long size, String hash) { this.size = size; this.hash = hash; }
    }

    private static final class ImportException extends IOException {
        final int message;
        ImportException(int message) { this.message = message; }
    }

    public WebImportViewModel(@NonNull Application application) { super(application); }

    public void load(Uri link) {
        cancelled.set(false);
        metadata = null;
        state.setValue(new State(Stage.LOADING, null, 0, R.string.web_import_loading));
        EXECUTOR.execute(() -> {
            try {
                cleanupInterruptedCopy();
                source = parseLink(link);
                metadata = readMetadata(source);
                checkCancelled();
                publish(Stage.READY, 0, R.string.web_import_confirm);
            } catch (Exception e) { fail(e); }
        });
    }

    public void start(Uri gamesFolder) {
        cancelled.set(false);
        state.setValue(new State(Stage.DOWNLOADING, metadata, 0, R.string.web_import_downloading));
        EXECUTOR.execute(() -> {
            File temporary = null;
            Exception failure = null;
            int result = R.string.web_import_complete;
            try {
                checkCancelled();
                DocumentFile games = Helper.getFileFromURI(getApplication(), gamesFolder);
                if (games == null || !games.isDirectory() || !games.canRead() || !games.canWrite()) {
                    throw new ImportException(R.string.web_import_storage_error);
                }
                if (games.findFile(metadata.fileName) != null) {
                    result = R.string.web_import_exists;
                } else {
                    File directory = cacheDirectory();
                    if (directory.getUsableSpace() < metadata.zipSize + 16 * 1024 * 1024) {
                        throw new ImportException(R.string.web_import_space_error);
                    }
                    temporary = File.createTempFile("download-", ".zip", directory);
                    download(temporary);
                    verify(temporary);
                    save(temporary, games);
                }
            } catch (Exception e) {
                failure = e;
            } finally {
                if (temporary != null && !temporary.delete()) temporary.deleteOnExit();
            }
            // A committed import is successful even if cancel is pressed just after rename.
            if (failure == null) publish(Stage.COMPLETE, 100, result);
            else fail(failure);
        });
    }

    public void cancel() {
        cancelled.set(true);
        HttpURLConnection active = connection;
        if (active != null) EXECUTOR_CANCEL.execute(active::disconnect);
    }
    private static final ExecutorService EXECUTOR_CANCEL = Executors.newSingleThreadExecutor();

    @Override protected void onCleared() { cancel(); }

    private void publish(Stage stage, int progress, int message) {
        state.postValue(new State(stage, metadata, progress, message));
    }

    private void fail(Exception e) {
        Log.w("KaiImport", "Import did not complete", e);
        publish(cancelled.get() ? Stage.CANCELLED : Stage.ERROR, 0,
                cancelled.get() ? R.string.web_import_cancelled :
                        e instanceof ImportException ? ((ImportException) e).message : R.string.web_import_failed);
    }

    private void checkCancelled() throws InterruptedIOException {
        if (cancelled.get()) throw new InterruptedIOException("Cancelled");
    }

    private static Uri parseLink(Uri link) throws ImportException {
        if (link == null || !"easyrpg-kai".equals(link.getScheme()) ||
                !"import".equals(link.getEncodedAuthority()) ||
                (link.getPath() != null && !link.getPath().isEmpty()) || link.getFragment() != null ||
                link.getQueryParameters("manifest").size() != 1 || link.toString().length() > 2048) {
            throw new ImportException(R.string.web_import_invalid_source);
        }
        Uri uri = Uri.parse(link.getQueryParameter("manifest"));
        validateSite(uri);
        if (!uri.getEncodedPath().matches("/api/archive-versions/[1-9][0-9]{0,15}/kai-import") ||
                uri.getQuery() != null) throw new ImportException(R.string.web_import_invalid_source);
        return uri;
    }

    private static void validateSite(Uri uri) throws ImportException {
        if (!"https".equals(uri.getScheme()) || uri.getUserInfo() != null || uri.getPort() != -1 ||
                uri.getFragment() != null ||
                !("viprpg-zh-archive.q578235562.workers.dev".equals(uri.getEncodedAuthority()) ||
                  "viprpg-zh-archive-staging.q578235562.workers.dev".equals(uri.getEncodedAuthority()))) {
            throw new ImportException(R.string.web_import_invalid_source);
        }
    }

    private HttpURLConnection connect(Uri uri, String accept) throws IOException {
        checkCancelled();
        HttpURLConnection c = (HttpURLConnection) new URL(uri.toString()).openConnection();
        connection = c;
        c.setInstanceFollowRedirects(false);
        c.setConnectTimeout(20000);
        c.setReadTimeout(20000);
        c.setRequestProperty("Accept", accept);
        c.setRequestProperty("Accept-Encoding", "identity");
        c.setRequestProperty("User-Agent", "EasyRPG-Player-Kai-Import/1");
        int status = c.getResponseCode();
        if (status != 200) {
            throw new ImportException(status == 404 ? R.string.web_import_unavailable :
                    status == 422 ? R.string.web_import_unsupported : R.string.web_import_network_error);
        }
        checkCancelled();
        return c;
    }

    private void disconnect() {
        HttpURLConnection active = connection;
        connection = null;
        if (active != null) active.disconnect();
    }

    private Metadata readMetadata(Uri uri) throws Exception {
        byte[] bytes;
        try {
            HttpURLConnection c = connect(uri, "application/json");
            try (InputStream in = c.getInputStream(); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[32768];
                int count;
                while ((count = in.read(buffer)) != -1) {
                    checkCancelled();
                    if (out.size() + count > MAX_METADATA) throw new ImportException(R.string.web_import_invalid_metadata);
                    out.write(buffer, 0, count);
                }
                bytes = out.toByteArray();
            }
        } finally { disconnect(); }
        try {
            JSONObject json = new JSONObject(new String(bytes, StandardCharsets.UTF_8));
            long id = json.getLong("archiveVersionId");
            long size = json.getLong("zipSizeBytes");
            String title = json.getString("title");
            String hash = json.getString("manifestSha256");
            String engine = json.getString("engineFamily");
            Uri download = Uri.parse(json.getString("downloadUrl"));
            validateSite(download);
            if (!"viprpg-kai.import.v1".equals(json.getString("schema")) ||
                    !uri.getHost().equals(download.getHost()) ||
                    !uri.getPath().equals("/api/archive-versions/" + id + "/kai-import") ||
                    !download.getPath().equals("/api/archive-versions/" + id + "/download") ||
                    size <= 0 || size > MAX_ZIP || title.isEmpty() || title.length() > 500 ||
                    !hash.matches("[0-9a-f]{64}") ||
                    !(engine.equals("rpg_maker_2000") || engine.equals("rpg_maker_2003") || engine.equals("rpg_maker_2003_maniac"))) {
                throw new ImportException(R.string.web_import_invalid_metadata);
            }
            JSONArray array = json.getJSONArray("files");
            if (array.length() == 0 || array.length() > MAX_FILES) throw new ImportException(R.string.web_import_invalid_metadata);
            Map<String, Entry> files = new HashMap<>();
            Set<String> normalized = new HashSet<>();
            long total = 0;
            for (int i = 0; i < array.length(); i++) {
                JSONObject file = array.getJSONObject(i);
                String path = file.getString("path");
                long fileSize = file.getLong("size");
                String fileHash = file.getString("sha256");
                validatePath(path);
                if (fileSize < 0 || fileSize > MAX_ZIP || !fileHash.matches("[0-9a-f]{64}") ||
                        !normalized.add(path.toLowerCase(Locale.ROOT))) throw new ImportException(R.string.web_import_invalid_metadata);
                total += fileSize;
                if (total > MAX_ZIP) throw new ImportException(R.string.web_import_invalid_metadata);
                files.put(path, new Entry(fileSize, fileHash));
            }
            if (!normalized.contains("rpg_rt.ldb") || !normalized.contains("rpg_rt.lmt")) {
                throw new ImportException(R.string.web_import_unsupported);
            }
            return new Metadata(title, id, hash, download, size, files);
        } catch (ImportException e) { throw e; }
        catch (Exception e) { throw new ImportException(R.string.web_import_invalid_metadata); }
    }

    private static void validatePath(String path) throws ImportException {
        if (path.length() > 1024 || path.startsWith("/") || path.contains("\\") ||
                path.contains(":") || path.matches("(?s).*[\\x00-\\x1f\\x7f].*")) {
            throw new ImportException(R.string.web_import_invalid_metadata);
        }
        for (String part : path.split("/", -1)) {
            if (part.isEmpty() || part.equals(".") || part.equals("..")) throw new ImportException(R.string.web_import_invalid_metadata);
        }
    }

    private void download(File target) throws Exception {
        try {
            HttpURLConnection c = connect(metadata.download, "application/zip");
            if (!metadata.manifestSha256.equals(c.getHeaderField("X-Manifest-SHA256")) ||
                    !Long.toString(metadata.zipSize).equals(c.getHeaderField("Content-Length"))) {
                throw new ImportException(R.string.web_import_changed);
            }
            try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(target)) {
                transfer(in, out, metadata.zipSize, Stage.DOWNLOADING, R.string.web_import_downloading, null);
            }
        } finally { disconnect(); }
    }

    private void verify(File file) throws Exception {
        publish(Stage.VERIFYING, 0, R.string.web_import_verifying);
        Set<String> seen = new HashSet<>();
        try (ZipFile zip = new ZipFile(file)) {
            if (zip.size() != metadata.files.size()) throw new ImportException(R.string.web_import_integrity_error);
            Enumeration<? extends ZipEntry> entries = zip.entries();
            int done = 0;
            while (entries.hasMoreElements()) {
                checkCancelled();
                ZipEntry entry = entries.nextElement();
                Entry expected = metadata.files.get(entry.getName());
                if (expected == null || !seen.add(entry.getName()) || entry.isDirectory() ||
                        entry.getSize() != expected.size || entry.getMethod() != ZipEntry.STORED) {
                    throw new ImportException(R.string.web_import_integrity_error);
                }
                MessageDigest digest = MessageDigest.getInstance("SHA-256");
                try (InputStream in = zip.getInputStream(entry)) {
                    transfer(in, null, expected.size, null, 0, digest);
                }
                if (!expected.hash.equals(hex(digest.digest()))) throw new ImportException(R.string.web_import_integrity_error);
                publish(Stage.VERIFYING, ++done * 100 / metadata.files.size(), R.string.web_import_verifying);
            }
        }
    }

    private void save(File file, DocumentFile games) throws Exception {
        checkCancelled();
        publish(Stage.SAVING, 0, R.string.web_import_saving);
        DocumentFile pending = games.createFile("application/octet-stream", ".kai-import-" + UUID.randomUUID() + ".part");
        if (pending == null) throw new ImportException(R.string.web_import_storage_error);
        boolean committed = false;
        SharedPreferences journal = journal();
        try {
            if (!journal.edit().putString("pending", pending.getUri().toString()).commit()) {
                throw new ImportException(R.string.web_import_storage_error);
            }
            MessageDigest original = MessageDigest.getInstance("SHA-256");
            try (InputStream in = new FileInputStream(file);
                 OutputStream out = getApplication().getContentResolver().openOutputStream(pending.getUri(), "wt")) {
                if (out == null) throw new ImportException(R.string.web_import_storage_error);
                transfer(in, out, metadata.zipSize, Stage.SAVING, R.string.web_import_saving, original);
            }
            // Verify the SAF provider stored all bytes, including the ZIP directory.
            MessageDigest stored = MessageDigest.getInstance("SHA-256");
            publish(Stage.SAVING, 100, R.string.web_import_verifying_storage);
            try (InputStream in = getApplication().getContentResolver().openInputStream(pending.getUri())) {
                if (in == null) throw new ImportException(R.string.web_import_storage_error);
                transfer(in, null, metadata.zipSize, null, 0, stored);
            }
            if (!MessageDigest.isEqual(original.digest(), stored.digest())) throw new ImportException(R.string.web_import_storage_error);
            checkCancelled();
            if (games.findFile(metadata.fileName) != null || !pending.renameTo(metadata.fileName)) {
                throw new ImportException(R.string.web_import_storage_error);
            }
            if (!metadata.fileName.equals(pending.getName())) throw new ImportException(R.string.web_import_storage_error);
            committed = true;
        } finally {
            if (committed || pending.delete()) journal.edit().remove("pending").commit();
        }
    }

    private void transfer(InputStream in, OutputStream out, long expected, Stage stage, int message,
                          MessageDigest digest) throws Exception {
        byte[] buffer = new byte[65536];
        long total = 0;
        int count, lastProgress = -1;
        while ((count = in.read(buffer)) != -1) {
            checkCancelled();
            total += count;
            if (total > expected) throw new ImportException(R.string.web_import_integrity_error);
            if (out != null) out.write(buffer, 0, count);
            if (digest != null) digest.update(buffer, 0, count);
            int progress = expected == 0 ? 100 : (int) (total * 100 / expected);
            if (stage != null && progress != lastProgress) {
                publish(stage, progress, message);
                lastProgress = progress;
            }
        }
        checkCancelled();
        if (total != expected) throw new ImportException(R.string.web_import_integrity_error);
    }

    private static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder();
        for (byte value : bytes) result.append(String.format(Locale.ROOT, "%02x", value & 255));
        return result.toString();
    }

    private SharedPreferences journal() { return getApplication().getSharedPreferences("web-import", 0); }

    private File cacheDirectory() throws IOException {
        File directory = new File(getApplication().getCacheDir(), "web-import");
        if (!directory.isDirectory() && !directory.mkdir()) throw new IOException("Cannot create import cache");
        return directory;
    }

    private void cleanupInterruptedCopy() throws Exception {
        SharedPreferences journal = journal();
        String uri = journal.getString("pending", null);
        if (uri != null) {
            DocumentFile pending = DocumentFile.fromSingleUri(getApplication(), Uri.parse(uri));
            if (pending != null && pending.exists()) {
                String name = pending.getName();
                // A process may have died after the final rename but before clearing the journal.
                if (name != null && name.startsWith(".kai-import-") && name.endsWith(".part") && !pending.delete()) {
                    throw new ImportException(R.string.web_import_storage_error);
                }
            }
            journal.edit().remove("pending").commit();
        }
        File[] files = cacheDirectory().listFiles();
        if (files != null) for (File file : files) {
            if (file.isFile() && file.getName().startsWith("download-") && !file.delete()) {
                throw new ImportException(R.string.web_import_storage_error);
            }
        }
    }
}
