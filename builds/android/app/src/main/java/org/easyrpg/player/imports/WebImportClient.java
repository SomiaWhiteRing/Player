package org.easyrpg.player.imports;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.SystemClock;
import android.net.Uri;

import androidx.documentfile.provider.DocumentFile;

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
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

/** Downloads only public archive snapshots from the two explicitly trusted site origins. */
public final class WebImportClient {
    private static final long MAX_ZIP = 1024L * 1024 * 1024;
    private static final int MAX_METADATA = 8 * 1024 * 1024;
    private static final int MAX_FILES = 50000;
    private final Context context;
    private final Progress progress;
    private final AtomicBoolean cancelled = new AtomicBoolean();
    private volatile HttpURLConnection connection;
    private Metadata metadata;
    private static final ExecutorService DISCONNECT = Executors.newCachedThreadPool();

    public enum Stage { LOADING, READY, NEEDS_FOLDER, EXISTS, QUEUED, DOWNLOADING, VERIFYING, SAVING, PAUSING, PAUSED, REMOVING, COMPLETE, ERROR }

    interface Progress {
        void update(Stage stage, int percent, long downloaded, long speed, int message);
        void pending(Uri uri) throws IOException;
    }

    WebImportClient(Context context, Progress progress) {
        this.context = context.getApplicationContext();
        this.progress = progress;
    }

    public static final class Metadata {
        public final String title, manifestSha256, fileName;
        public final Uri download, coverUrl;
        public final long workId;
        final String json;
        public final long archiveVersionId, zipSize;
        final Map<String, Entry> files;
        Metadata(String title, long id, String hash, Uri download, long zipSize, Map<String, Entry> files, long workId, Uri coverUrl, String json) {
            this.title = title;
            this.workId = workId;
            this.coverUrl = coverUrl;
            this.json = json;
            this.archiveVersionId = id;
            this.manifestSha256 = hash;
            this.download = download;
            this.zipSize = zipSize;
            this.files = files;
            // Stable per site + snapshot; staging IDs must never collide with production IDs.
            String site = "staging.viprpg.org".equals(download.getHost()) ? "staging-" : "";
            this.fileName = "VIPRPG-" + site + id + "-" + hash.substring(0, 16) + ".zip";
        }
    }

    private static final class Entry {
        final long size;
        final String hash;
        Entry(long size, String hash) { this.size = size; this.hash = hash; }
    }

    static final class ImportException extends IOException {
        final int message;
        ImportException(int message) { this.message = message; }
    }

    Metadata load(Uri link) throws Exception {
        metadata = readMetadata(parseLink(link));
        return metadata;
    }

    void install(Metadata metadata, File temporary, DocumentFile games, String pendingName) throws Exception {
        this.metadata = metadata;
        checkCancelled();
        if (temporary.getParentFile().getUsableSpace() < Math.max(0, metadata.zipSize - temporary.length()) + 16 * 1024 * 1024) {
            throw new ImportException(R.string.web_import_space_error);
        }
        download(temporary);
        verify(temporary);
        save(temporary, games, pendingName);
    }

    public void cancel() {
        cancelled.set(true);
        HttpURLConnection active = connection;
        if (active != null) DISCONNECT.execute(active::disconnect);
    }

    private void publish(Stage stage, int percent, int message) {
        if (progress != null) progress.update(stage, percent, metadata.zipSize, 0, message);
    }

    private void checkCancelled() throws InterruptedIOException {
        if (cancelled.get()) throw new InterruptedIOException("Paused");
    }

    static Uri parseLink(Uri link) throws ImportException {
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
                !("viprpg.org".equals(uri.getEncodedAuthority()) ||
                  "staging.viprpg.org".equals(uri.getEncodedAuthority()))) {
            throw new ImportException(R.string.web_import_invalid_source);
        }
    }

    private HttpURLConnection connect(Uri uri, String accept) throws IOException {
        return connect(uri, accept, 0);
    }

    private HttpURLConnection connect(Uri uri, String accept, long offset) throws IOException {
        checkCancelled();
        HttpURLConnection c = (HttpURLConnection) new URL(uri.toString()).openConnection();
        connection = c;
        c.setInstanceFollowRedirects(false);
        c.setConnectTimeout(20000);
        c.setReadTimeout(20000);
        c.setRequestProperty("Accept", accept);
        c.setRequestProperty("Accept-Encoding", "identity");
        c.setRequestProperty("User-Agent", "EasyRPG-Player-Kai-Import/1");
        if (offset > 0) c.setRequestProperty("Range", "bytes=" + offset + "-");
        int status = c.getResponseCode();
        if (status != 200 && !(offset > 0 && status == 206)) {
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
        return parseMetadata(uri, new String(bytes, StandardCharsets.UTF_8));
    }

    static Metadata parseMetadata(Uri uri, String text) throws Exception {
        try {
            JSONObject json = new JSONObject(text);
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
            long workId = json.optLong("workId", 0);
            if (workId < 0) throw new ImportException(R.string.web_import_invalid_metadata);
            Uri cover = null;
            String coverValue = json.optString("coverUrl", "");
            if (!coverValue.isEmpty() && !coverValue.equals("null")) {
                cover = Uri.parse(coverValue);
                validateSite(cover);
                if (!uri.getHost().equals(cover.getHost()) ||
                        !cover.getEncodedPath().matches("/api/media/blobs/[0-9a-f]{64}") || cover.getQuery() != null) {
                    throw new ImportException(R.string.web_import_invalid_metadata);
                }
            }
            return new Metadata(title, id, hash, download, size, files, workId, cover, text);
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

    Bitmap readCover(Metadata metadata) throws IOException {
        if (metadata.coverUrl == null) return null;
        try {
            HttpURLConnection c = connect(metadata.coverUrl, "image/*");
            try (InputStream in = c.getInputStream(); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[32768];
                int count;
                while ((count = in.read(buffer)) != -1) {
                    checkCancelled();
                    if (out.size() + count > MAX_METADATA) throw new IOException("Cover is too large");
                    out.write(buffer, 0, count);
                }
                byte[] bytes = out.toByteArray();
                BitmapFactory.Options options = new BitmapFactory.Options();
                options.inJustDecodeBounds = true;
                BitmapFactory.decodeByteArray(bytes, 0, bytes.length, options);
                if (options.outWidth <= 0 || options.outHeight <= 0) return null;
                options.inSampleSize = 1;
                while (Math.max(options.outWidth, options.outHeight) / options.inSampleSize > 1024) options.inSampleSize *= 2;
                options.inJustDecodeBounds = false;
                return BitmapFactory.decodeByteArray(bytes, 0, bytes.length, options);
            }
        } finally { disconnect(); }
    }

    private void download(File target) throws Exception {
        long offset = target.length();
        if (offset == metadata.zipSize) return;
        if (offset > metadata.zipSize) throw new ImportException(R.string.web_import_integrity_error);
        try {
            HttpURLConnection c = connect(metadata.download, "application/zip", offset);
            String builder = metadata.download.getQueryParameter("zip_builder");
            if (!metadata.manifestSha256.equals(c.getHeaderField("X-Manifest-SHA256")) ||
                    (builder != null && !builder.equals(c.getHeaderField("X-Download-Zip-Builder")))) {
                throw new ImportException(R.string.web_import_changed);
            }
            if (c.getResponseCode() == 206) {
                String range = "bytes " + offset + "-" + (metadata.zipSize - 1) + "/" + metadata.zipSize;
                if (!range.equals(c.getHeaderField("Content-Range"))) throw new ImportException(R.string.web_import_changed);
            } else {
                // A server that ignores Range must replace, never append to, the partial file.
                offset = 0;
            }
            if (!Long.toString(metadata.zipSize - offset).equals(c.getHeaderField("Content-Length"))) {
                throw new ImportException(R.string.web_import_changed);
            }
            long total = offset, sampleBytes = offset, sampleTime = SystemClock.elapsedRealtime();
            progress.update(Stage.DOWNLOADING, (int) (total * 100 / metadata.zipSize), total, 0, R.string.web_import_downloading);
            try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(target, offset > 0)) {
                byte[] buffer = new byte[65536];
                int count;
                while ((count = in.read(buffer)) != -1) {
                    checkCancelled();
                    if (total + count > metadata.zipSize) throw new ImportException(R.string.web_import_integrity_error);
                    out.write(buffer, 0, count);
                    total += count;
                    long now = SystemClock.elapsedRealtime();
                    if (now - sampleTime >= 500 || total == metadata.zipSize) {
                        long speed = (total - sampleBytes) * 1000 / Math.max(1, now - sampleTime);
                        progress.update(Stage.DOWNLOADING, (int) (total * 100 / metadata.zipSize), total, speed, R.string.web_import_downloading);
                        sampleTime = now;
                        sampleBytes = total;
                    }
                }
            }
            checkCancelled();
            // Keep a valid prefix when the connection ends early so Retry can resume it.
            if (total != metadata.zipSize) throw new ImportException(R.string.web_import_network_error);
        } finally { disconnect(); }
    }

    private void verify(File file) throws Exception {
        publish(Stage.VERIFYING, 0, R.string.web_import_verifying);
        Set<String> seen = new HashSet<>();
        try (ZipFile zip = new ZipFile(file)) {
            if (zip.size() != metadata.files.size()) throw new ImportException(R.string.web_import_integrity_error);
            Enumeration<? extends ZipEntry> entries = zip.entries();
            int done = 0, lastProgress = -1;
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
                int percent = ++done * 100 / metadata.files.size();
                if (percent != lastProgress) publish(Stage.VERIFYING, percent, R.string.web_import_verifying);
                lastProgress = percent;
            }
        } catch (ZipException e) {
            throw new ImportException(R.string.web_import_integrity_error);
        }
    }

    private void save(File file, DocumentFile games, String pendingName) throws Exception {
        checkCancelled();
        publish(Stage.SAVING, 0, R.string.web_import_saving);
        DocumentFile pending = games.createFile("application/octet-stream", pendingName);
        if (pending == null) throw new ImportException(R.string.web_import_storage_error);
        boolean committed = false;
        boolean renamed = false;
        try {
            progress.pending(pending.getUri());
            MessageDigest original = MessageDigest.getInstance("SHA-256");
            try (InputStream in = new FileInputStream(file);
                 OutputStream out = context.getContentResolver().openOutputStream(pending.getUri(), "wt")) {
                if (out == null) throw new ImportException(R.string.web_import_storage_error);
                transfer(in, out, metadata.zipSize, Stage.SAVING, R.string.web_import_saving, original);
            }
            // Verify the SAF provider stored all bytes, including the ZIP directory.
            MessageDigest stored = MessageDigest.getInstance("SHA-256");
            publish(Stage.SAVING, 100, R.string.web_import_verifying_storage);
            try (InputStream in = context.getContentResolver().openInputStream(pending.getUri())) {
                if (in == null) throw new ImportException(R.string.web_import_storage_error);
                transfer(in, null, metadata.zipSize, null, 0, stored);
            }
            if (!MessageDigest.isEqual(original.digest(), stored.digest())) throw new ImportException(R.string.web_import_storage_error);
            checkCancelled();
            if (games.findFile(metadata.fileName) != null || !pending.renameTo(metadata.fileName)) {
                throw new ImportException(R.string.web_import_storage_error);
            }
            renamed = true;
            if (!metadata.fileName.equals(pending.getName())) throw new ImportException(R.string.web_import_storage_error);
            committed = true;
        } finally {
            // Never delete a game after a successful rename, even if a provider reports an unexpected name.
            if (committed || renamed || pending.delete()) progress.pending(null);
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

}
