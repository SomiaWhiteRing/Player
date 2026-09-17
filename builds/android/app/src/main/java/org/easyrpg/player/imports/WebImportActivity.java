package org.easyrpg.player.imports;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.format.Formatter;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.documentfile.provider.DocumentFile;
import androidx.lifecycle.ViewModelProvider;

import org.easyrpg.player.BaseActivity;
import org.easyrpg.player.Helper;
import org.easyrpg.player.InitActivity;
import org.easyrpg.player.R;
import org.easyrpg.player.game_browser.GameBrowserActivity;
import org.easyrpg.player.game_browser.GameBrowserHelper;
import org.easyrpg.player.settings.SettingsManager;

/** User confirmation is required before downloading any game payload. */
public class WebImportActivity extends BaseActivity {
    private WebImportViewModel model;
    private Button action, cancel;
    private TextView details, status;
    private ProgressBar progress;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_web_import);
        details = findViewById(R.id.import_details);
        status = findViewById(R.id.import_status);
        action = findViewById(R.id.import_action);
        cancel = findViewById(R.id.import_cancel);
        progress = findViewById(R.id.import_progress);
        model = new ViewModelProvider(this).get(WebImportViewModel.class);
        model.state.observe(this, this::render);
        action.setOnClickListener(v -> act());
        cancel.setOnClickListener(v -> backPressed());
        if (model.state.getValue() == null) model.load(incomingLink(getIntent()));
    }

    @Override public void onResume() {
        super.onResume();
        onBackPressedCallback.setEnabled(true);
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        WebImportViewModel.State state = model.state.getValue();
        if (state != null && state.busy()) {
            Toast.makeText(this, R.string.web_import_busy, Toast.LENGTH_LONG).show();
            return;
        }
        setIntent(intent);
        model.load(incomingLink(intent));
    }

    private Uri incomingLink(Intent intent) {
        return Intent.ACTION_VIEW.equals(intent.getAction()) ? intent.getData() : null;
    }

    private DocumentFile gamesFolder() {
        return Helper.getFileFromURI(this, SettingsManager.getGamesFolderURI(this));
    }

    private void render(WebImportViewModel.State state) {
        if (state.stage == WebImportViewModel.Stage.COMPLETE) {
            SettingsManager.clearGamesCache();
            GameBrowserActivity.resetGamesList();
        }
        if (state.metadata != null) {
            details.setText(getString(R.string.web_import_details, state.metadata.title,
                    Long.toString(state.metadata.archiveVersionId),
                    Formatter.formatFileSize(this, state.metadata.zipSize),
                    state.metadata.download.getHost(), state.metadata.fileName));
        } else details.setText(R.string.web_import_intro);
        status.setText(state.message);
        progress.setVisibility(state.busy() ? View.VISIBLE : View.GONE);
        progress.setIndeterminate(state.stage == WebImportViewModel.Stage.LOADING);
        progress.setProgress(state.progress);
        action.setVisibility(state.busy() ? View.GONE : View.VISIBLE);
        action.setText(state.stage == WebImportViewModel.Stage.READY ? R.string.web_import_start :
                state.stage == WebImportViewModel.Stage.COMPLETE ? R.string.web_import_open_games : R.string.web_import_retry);
        cancel.setEnabled(true);
        cancel.setText(state.busy() ? R.string.cancel : R.string.web_import_close);
    }

    private void act() {
        WebImportViewModel.State state = model.state.getValue();
        if (state == null || state.busy()) return;
        if (state.stage == WebImportViewModel.Stage.COMPLETE) {
            GameBrowserActivity.resetGamesList();
            startActivity(new Intent(this, InitActivity.class));
            finish();
        } else if (state.stage == WebImportViewModel.Stage.READY) {
            DocumentFile games = gamesFolder();
            if (games == null || !games.canRead() || !games.canWrite()) {
                GameBrowserHelper.pickAGamesFolder(this);
            } else {
                model.start(games.getUri());
            }
        } else {
            model.load(incomingLink(getIntent()));
        }
    }

    @Override public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != GameBrowserHelper.FOLDER_HAS_BEEN_CHOSEN) return;
        try {
            GameBrowserHelper.SafError error = GameBrowserHelper.dealAfterFolderSelected(this, requestCode, resultCode, data);
            if (error == GameBrowserHelper.SafError.OK) {
                // Show confirmation again with the selected destination before downloading.
                status.setText(R.string.web_import_folder_ready);
            } else if (error != GameBrowserHelper.SafError.ABORTED) {
                GameBrowserHelper.showErrorMessage(this, error);
            }
        } catch (RuntimeException e) {
            status.setText(R.string.web_import_storage_error);
        }
    }

    @Override public void backPressed() {
        WebImportViewModel.State state = model.state.getValue();
        if (state != null && state.busy()) {
            model.cancel();
            cancel.setEnabled(false);
            status.setText(R.string.web_import_cancelling);
        } else finish();
    }
}
