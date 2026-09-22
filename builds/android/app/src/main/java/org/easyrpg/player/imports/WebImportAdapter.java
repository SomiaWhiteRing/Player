package org.easyrpg.player.imports;

import android.text.format.Formatter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.easyrpg.player.R;
import org.easyrpg.player.imports.WebImportClient.Stage;
import org.easyrpg.player.imports.WebImportDownloads.Download;

import java.util.ArrayList;
import java.util.List;

/** Pending cards share the game list but expose no launch, settings or favorite actions. */
public final class WebImportAdapter extends RecyclerView.Adapter<WebImportAdapter.Holder> {
    private final List<Download> downloads = new ArrayList<>();

    public void submit(List<Download> snapshot) {
        List<Download> pending = new ArrayList<>();
        for (Download download : snapshot) if (download.stage != Stage.COMPLETE) pending.add(download);
        boolean sameIds = pending.size() == downloads.size();
        for (int i = 0; sameIds && i < pending.size(); i++) sameIds = pending.get(i).id.equals(downloads.get(i).id);
        downloads.clear();
        downloads.addAll(pending);
        if (sameIds) notifyItemRangeChanged(0, downloads.size(), "progress");
        else notifyDataSetChanged();
    }

    @Override public int getItemCount() { return downloads.size(); }

    @NonNull @Override public Holder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        return new Holder(LayoutInflater.from(parent.getContext()).inflate(R.layout.browser_download_card, parent, false));
    }

    @Override public void onBindViewHolder(@NonNull Holder holder, int position) {
        Download download = downloads.get(position);
        android.content.Context context = holder.itemView.getContext();
        holder.title.setText(download.metadata.title);
        holder.title.setEnabled(false);
        holder.cover.setEnabled(false);
        holder.cover.setAlpha(0.5f);
        holder.cover.setImageBitmap(download.cover);
        if (download.cover == null) holder.cover.setImageResource(R.drawable.ic_gamepad_black);
        holder.status.setText(download.message);
        holder.progress.setIndeterminate(download.stage == Stage.QUEUED || download.stage == Stage.PAUSING);
        holder.progress.setProgress(download.percent);
        holder.transfer.setText(context.getString(R.string.web_import_transfer, download.percent,
                Formatter.formatFileSize(context, download.bytes), Formatter.formatFileSize(context, download.metadata.zipSize),
                Formatter.formatFileSize(context, download.speed)));
        holder.action.setEnabled(download.stage != Stage.PAUSING && download.stage != Stage.REMOVING);
        holder.action.setText(download.resumable() ? R.string.web_import_resume : R.string.web_import_pause);
        holder.remove.setVisibility(download.resumable() ? View.VISIBLE : View.GONE);
        holder.remove.setOnClickListener(v -> WebImportDownloads.get(context).remove(download.id));
        holder.action.setOnClickListener(v -> {
            WebImportDownloads queue = WebImportDownloads.get(context);
            if (download.resumable()) {
                queue.resume(download.id);
                WebImportService.start(context);
            } else queue.pause(download.id);
        });
    }

    static final class Holder extends RecyclerView.ViewHolder {
        final TextView title, status, transfer;
        final ImageView cover;
        final ProgressBar progress;
        final Button action, remove;
        Holder(View view) {
            super(view);
            title = view.findViewById(R.id.download_title);
            status = view.findViewById(R.id.download_status);
            transfer = view.findViewById(R.id.download_transfer);
            cover = view.findViewById(R.id.download_cover);
            progress = view.findViewById(R.id.download_progress);
            action = view.findViewById(R.id.download_action);
            remove = view.findViewById(R.id.download_remove);
        }
    }
}
