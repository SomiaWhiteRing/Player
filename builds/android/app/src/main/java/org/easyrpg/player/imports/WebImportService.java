package org.easyrpg.player.imports;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.text.format.Formatter;
import android.util.Log;

import androidx.core.app.NotificationCompat;
import androidx.core.app.ServiceCompat;
import androidx.core.content.ContextCompat;
import androidx.lifecycle.Observer;

import org.easyrpg.player.InitActivity;
import org.easyrpg.player.R;
import org.easyrpg.player.imports.WebImportClient.Stage;
import org.easyrpg.player.imports.WebImportDownloads.Download;

import java.util.List;

/** Foreground data-sync service keeps downloading when the app is in the background. */
public class WebImportService extends Service {
    private static final String CHANNEL = "game-downloads";
    private static final int NOTIFICATION = 4101;
    private WebImportDownloads downloads;
    private PowerManager.WakeLock wakeLock;
    private boolean foregroundStarted;
    private final Observer<List<Download>> observer = ignored -> updateNotification();

    public static void start(Context context) {
        try { ContextCompat.startForegroundService(context, new Intent(context, WebImportService.class)); }
        catch (RuntimeException e) {
            Log.w("KaiImport", "Cannot start background download", e);
            WebImportDownloads.get(context).startFailed();
        }
    }

    @Override public void onCreate() {
        super.onCreate();
        downloads = WebImportDownloads.get(this);
        NotificationManager notifications = getSystemService(NotificationManager.class);
        if (Build.VERSION.SDK_INT >= 26) {
            notifications.createNotificationChannel(new NotificationChannel(CHANNEL,
                    getString(R.string.web_import_download_channel), NotificationManager.IMPORTANCE_LOW));
        }
        try {
            ServiceCompat.startForeground(this, NOTIFICATION, notification(null),
                    Build.VERSION.SDK_INT >= 29 ? ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC : 0);
            foregroundStarted = true;
        } catch (RuntimeException e) {
            Log.w("KaiImport", "Foreground downloads are unavailable", e);
            downloads.startFailed();
            stopSelf();
            return;
        }
        PowerManager power = (PowerManager) getSystemService(POWER_SERVICE);
        wakeLock = power.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, getPackageName() + ":game-downloads");
        wakeLock.acquire(6 * 60 * 60 * 1000L);
        downloads.observe().observeForever(observer);
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (!foregroundStarted) return START_NOT_STICKY;
        if (intent != null && "pause".equals(intent.getAction())) {
            downloads.pause(intent.getStringExtra("download"));
        }
        downloads.startQueued();
        updateNotification();
        return START_STICKY;
    }

    private void updateNotification() {
        if (!foregroundStarted) return;
        Download active = null;
        for (Download download : downloads.snapshot()) {
            if (download.active()) {
                if (active == null || download.stage != Stage.QUEUED) active = download;
                if (download.stage != Stage.QUEUED) break;
            }
        }
        if (active == null) {
            ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE);
            stopSelf();
        } else {
            getSystemService(NotificationManager.class).notify(NOTIFICATION, notification(active));
        }
    }

    private Notification notification(Download active) {
        Intent open = new Intent(this, InitActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, CHANNEL)
                .setSmallIcon(android.R.drawable.stat_sys_download)
                .setContentTitle(active == null ? getString(R.string.web_import_title) : active.metadata.title)
                .setContentText(active == null ? getString(R.string.web_import_queued) : getString(active.message))
                .setContentIntent(PendingIntent.getActivity(this, 0, open, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE))
                .setOngoing(true).setOnlyAlertOnce(true).setCategory(NotificationCompat.CATEGORY_PROGRESS)
                .setProgress(100, active == null ? 0 : active.percent, active == null);
        if (active != null) {
            builder.setSubText(getString(R.string.web_import_transfer, active.percent,
                    Formatter.formatFileSize(this, active.bytes), Formatter.formatFileSize(this, active.metadata.zipSize),
                    Formatter.formatFileSize(this, active.speed)));
            Intent pause = new Intent(this, WebImportService.class).setAction("pause").putExtra("download", active.id);
            builder.addAction(android.R.drawable.ic_media_pause, getString(R.string.web_import_pause),
                    PendingIntent.getService(this, 1, pause, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE));
        }
        return builder.build();
    }

    @Override public void onTimeout(int startId, int foregroundServiceType) {
        downloads.pauseAll();
        ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE);
        stopSelf();
    }

    @Override public void onDestroy() {
        downloads.observe().removeObserver(observer);
        downloads.pauseAll();
        if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
        super.onDestroy();
    }

    @Override public IBinder onBind(Intent intent) { return null; }
}
