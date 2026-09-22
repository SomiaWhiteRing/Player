package org.easyrpg.player.imports;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.net.Uri;
import android.os.Bundle;
import android.text.format.Formatter;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.documentfile.provider.DocumentFile;
import androidx.lifecycle.ViewModelProvider;

import org.easyrpg.player.BaseActivity;
import org.easyrpg.player.Helper;
import org.easyrpg.player.R;
import org.easyrpg.player.game_browser.GameBrowserActivity;
import org.easyrpg.player.imports.WebImportClient.Stage;
import org.easyrpg.player.game_browser.GameBrowserHelper;
import org.easyrpg.player.settings.SettingsManager;

/** User confirmation is required before downloading any game payload. */
public class WebImportActivity extends BaseActivity {
    private WebImportViewModel model;
    private Button action, cancel;
    private TextView details, status;
    private ProgressBar progress;
    private ImageView cover;
    private boolean openingDownloads;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        openingDownloads = savedInstanceState != null && savedInstanceState.getBoolean("openingDownloads");
        setContentView(R.layout.activity_web_import);
        cover = findViewById(R.id.import_cover);
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
        model.refreshLocal();
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
        if (state.stage == Stage.QUEUED && !openingDownloads) {
            openingDownloads = true;
            if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[] {Manifest.permission.POST_NOTIFICATIONS}, 41);
            } else openDownloads();
        }
        if (state.metadata != null) {
            details.setText(getString(R.string.web_import_details, state.metadata.title,
                    Long.toString(state.metadata.archiveVersionId),
                    Formatter.formatFileSize(this, state.metadata.zipSize),
                    state.metadata.download.getHost(), state.metadata.fileName));
        } else details.setText(R.string.web_import_intro);
        cover.setImageBitmap(state.cover);
        if (state.cover == null) cover.setImageResource(R.drawable.ic_gamepad_black);
        cover.setVisibility(state.metadata == null ? View.GONE : View.VISIBLE);
        status.setText(state.message);
        progress.setVisibility(state.busy() ? View.VISIBLE : View.GONE);
        progress.setIndeterminate(true);
        action.setVisibility(state.busy() || state.stage == Stage.EXISTS ? View.GONE : View.VISIBLE);
        action.setText(state.stage == Stage.READY ? R.string.web_import_start : R.string.web_import_retry);
        cancel.setEnabled(true);
        cancel.setText(R.string.web_import_close);
    }

    private void act() {
        WebImportViewModel.State state = model.state.getValue();
        if (state == null || state.busy()) return;
        if (state.stage == Stage.READY) {
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

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        // Android allows foreground downloads even if notification permission is declined.
        if (requestCode == 41) openDownloads();
    }

    @Override protected void onSaveInstanceState(Bundle outState) {
        outState.putBoolean("openingDownloads", openingDownloads);
        super.onSaveInstanceState(outState);
    }

    private void openDownloads() {
        WebImportService.start(this);
        startActivity(new Intent(this, GameBrowserActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT));
        finish();
    }

    @Override public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != GameBrowserHelper.FOLDER_HAS_BEEN_CHOSEN) return;
        try {
            GameBrowserHelper.SafError error = GameBrowserHelper.dealAfterFolderSelected(this, requestCode, resultCode, data);
            if (error == GameBrowserHelper.SafError.OK) {
                // Show confirmation again with the selected destination before downloading.
                model.refreshLocal();
            } else if (error != GameBrowserHelper.SafError.ABORTED) {
                GameBrowserHelper.showErrorMessage(this, error);
            }
        } catch (RuntimeException e) {
            status.setText(R.string.web_import_storage_error);
        }
    }

    @Override public void backPressed() { finish(); }
}
