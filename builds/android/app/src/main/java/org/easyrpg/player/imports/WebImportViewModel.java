package org.easyrpg.player.imports;

import android.app.Application;
import android.graphics.Bitmap;
import android.net.Uri;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.documentfile.provider.DocumentFile;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.MutableLiveData;

import org.easyrpg.player.Helper;
import org.easyrpg.player.R;
import org.easyrpg.player.imports.WebImportClient.Metadata;
import org.easyrpg.player.imports.WebImportClient.Stage;
import org.easyrpg.player.settings.SettingsManager;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Only loads the confirmation preview. Downloads belong to WebImportService. */
public class WebImportViewModel extends AndroidViewModel {
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private volatile WebImportClient client;
    private volatile int generation;
    private Uri link;
    public final MutableLiveData<State> state = new MutableLiveData<>();

    public static final class State {
        public final Stage stage;
        public final Metadata metadata;
        public final Bitmap cover;
        public final int message;
        State(Stage stage, Metadata metadata, Bitmap cover, int message) {
            this.stage = stage;
            this.metadata = metadata;
            this.cover = cover;
            this.message = message;
        }
        public boolean busy() { return stage == Stage.LOADING || stage == Stage.QUEUED; }
    }

    public WebImportViewModel(@NonNull Application application) { super(application); }

    public void load(Uri link) {
        this.link = link;
        int request = ++generation;
        WebImportClient previous = client;
        if (previous != null) previous.cancel();
        state.setValue(new State(Stage.LOADING, null, null, R.string.web_import_loading));
        executor.execute(() -> {
            WebImportClient loader = new WebImportClient(getApplication(), null);
            client = loader;
            try {
                Metadata metadata = loader.load(link);
                Log.i("KaiImport", "Preview archive=" + metadata.archiveVersionId + " source=" + metadata.download.getHost()
                        + " destination=games/" + metadata.fileName + " size=" + metadata.zipSize);
                Bitmap cover = null;
                try { cover = loader.readCover(metadata); }
                catch (Exception e) { Log.w("KaiImport", "Cover unavailable", e); }
                State result = check(metadata, cover);
                if (request == generation) state.postValue(result);
            } catch (Exception e) {
                Log.w("KaiImport", "Cannot load import preview", e);
                if (request == generation) state.postValue(new State(Stage.ERROR, null, null,
                        e instanceof WebImportClient.ImportException ? ((WebImportClient.ImportException) e).message : R.string.web_import_failed));
            }
        });
    }

    private State check(Metadata metadata, Bitmap cover) {
        Uri folder = SettingsManager.getGamesFolderURI(getApplication());
        DocumentFile games = Helper.getFileFromURI(getApplication(), folder);
        WebImportDownloads downloads = WebImportDownloads.get(getApplication());
        boolean installed = downloads.isInstalled(metadata, games);
        boolean pending = downloads.contains(metadata, folder);
        if (installed || pending) return new State(Stage.EXISTS, metadata, cover,
                installed ? R.string.web_import_exists : R.string.web_import_already_queued);
        if (games == null || !games.isDirectory() || !games.canRead() || !games.canWrite()) {
            return new State(Stage.NEEDS_FOLDER, metadata, cover, R.string.web_import_choose_folder_prompt);
        }
        return new State(Stage.READY, metadata, cover, 0);
    }

    public void refreshLocal() {
        State current = state.getValue();
        if (current == null || current.metadata == null || current.busy()) return;
        int request = ++generation;
        state.setValue(new State(Stage.LOADING, current.metadata, current.cover, R.string.web_import_checking_local));
        executor.execute(() -> {
            try {
                State result = check(current.metadata, current.cover);
                if (request == generation) state.postValue(result);
            } catch (RuntimeException e) {
                Log.w("KaiImport", "Cannot check game folder", e);
                if (request == generation) state.postValue(new State(Stage.ERROR, current.metadata, current.cover, R.string.web_import_storage_error));
            }
        });
    }

    public void start(Uri folder) {
        State current = state.getValue();
        if (current == null || current.stage != Stage.READY) return;
        int request = ++generation;
        state.setValue(new State(Stage.LOADING, current.metadata, current.cover, R.string.web_import_checking_local));
        executor.execute(() -> {
            try {
                State checked = check(current.metadata, current.cover);
                if (request != generation) return;
                if (checked.stage != Stage.READY) { state.postValue(checked); return; }
                WebImportDownloads.get(getApplication()).enqueue(link, current.metadata, current.cover, folder);
                if (request == generation) state.postValue(new State(Stage.QUEUED, current.metadata, current.cover, R.string.web_import_queued));
            } catch (Exception e) {
                Log.w("KaiImport", "Cannot queue import", e);
                if (request == generation) state.postValue(new State(Stage.ERROR, current.metadata, current.cover, R.string.web_import_failed));
            }
        });
    }

    public void storageError(RuntimeException error) {
        Log.w("KaiImport", "Cannot select game folder", error);
        ++generation;
        State current = state.getValue();
        state.setValue(new State(Stage.ERROR, current == null ? null : current.metadata,
                current == null ? null : current.cover, R.string.web_import_storage_error));
    }

    @Override protected void onCleared() {
        ++generation;
        if (client != null) client.cancel();
        executor.shutdownNow();
    }
}
