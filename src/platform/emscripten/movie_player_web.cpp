/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#include "platform/emscripten/movie_player_web.h"

#include <algorithm>

#include <emscripten.h>

EM_JS(int, EasyRpgWebMovieOpen, (const char* path), {
	const filePath = UTF8ToString(path);
	try {
		if (!Module.webMovie) {
			Module.webMovie = {};
		}

		if (!Module.webMovie.element) {
			const video = document.createElement('video');
			video.id = 'easyrpg-movie';
			video.preload = 'auto';
			video.playsInline = true;
			video.setAttribute('playsinline', "");
			video.style.position = 'absolute';
			video.style.pointerEvents = 'none';
			video.style.display = 'none';
			video.style.backgroundColor = 'black';
			video.style.zIndex = '1';

			const canvas = Module.canvas || document.getElementById('canvas');
			const parent = canvas && canvas.parentNode ? canvas.parentNode : document.body;
			if (getComputedStyle(parent).position === 'static') {
				parent.style.position = 'relative';
			}
			parent.appendChild(video);

			Module.webMovie.element = video;
			Module.webMovie.container = parent;
		}

		const video = Module.webMovie.element;
		if (Module.webMovie.objectUrl) {
			URL.revokeObjectURL(Module.webMovie.objectUrl);
			Module.webMovie.objectUrl = undefined;
		}

		video.pause();
		video.removeAttribute('src');
		video.load();

		const data = FS.readFile(filePath);
		const lowerPath = filePath.toLowerCase();
		let mimeType = "";
		if (lowerPath.endsWith('.mp4') || lowerPath.endsWith('.m4v')) {
			mimeType = 'video/mp4';
		} else if (lowerPath.endsWith('.webm')) {
			mimeType = 'video/webm';
		} else if (lowerPath.endsWith('.ogv') || lowerPath.endsWith('.ogg')) {
			mimeType = 'video/ogg';
		} else if (lowerPath.endsWith('.mpg') || lowerPath.endsWith('.mpeg')) {
			mimeType = 'video/mpeg';
		} else if (lowerPath.endsWith('.avi')) {
			mimeType = 'video/x-msvideo';
		}
		const blob = new Blob([data], mimeType ? { type: mimeType } : undefined);
		const objectUrl = URL.createObjectURL(blob);
		Module.webMovie.objectUrl = objectUrl;
		Module.webMovie.playing = true;
		Module.webMovie.ended = false;
		Module.webMovie.error = "";

		video.onended = () => {
			Module.webMovie.playing = false;
			Module.webMovie.ended = true;
			video.style.display = 'none';
		};
		video.onerror = () => {
			Module.webMovie.playing = false;
			Module.webMovie.error = video.error ? String(video.error.code) : 'unknown';
			video.style.display = 'none';
		};

		video.src = objectUrl;
		video.style.display = 'block';
		const promise = video.play();
		if (promise && promise.catch) {
			promise.catch((error) => {
				Module.webMovie.playing = false;
				Module.webMovie.error = error && error.message ? error.message : String(error);
				video.style.display = 'none';
			});
		}
		return 1;
	} catch (error) {
		if (Module.webMovie) {
			Module.webMovie.playing = false;
			Module.webMovie.error = error && error.message ? error.message : String(error);
		}
		return 0;
	}
});

EM_JS(void, EasyRpgWebMovieUpdate, (int x, int y, int width, int height), {
	if (!Module.webMovie || !Module.webMovie.element) {
		return;
	}

	const video = Module.webMovie.element;
	const canvas = Module.canvas || document.getElementById('canvas');
	if (!canvas) {
		return;
	}

	const bounds = canvas.getBoundingClientRect();
	const container = Module.webMovie.container || video.parentNode || document.body;
	const containerBounds = container.getBoundingClientRect();
	const scaleX = bounds.width / Math.max(canvas.width || bounds.width, 1);
	const scaleY = bounds.height / Math.max(canvas.height || bounds.height, 1);

	video.style.left = `${bounds.left - containerBounds.left + x * scaleX}px`;
	video.style.top = `${bounds.top - containerBounds.top + y * scaleY}px`;
	video.style.width = `${Math.max(width, 1) * scaleX}px`;
	video.style.height = `${Math.max(height, 1) * scaleY}px`;
});

EM_JS(void, EasyRpgWebMovieStop, (), {
	if (!Module.webMovie || !Module.webMovie.element) {
		return;
	}

	const video = Module.webMovie.element;
	video.pause();
	video.style.display = 'none';
	video.removeAttribute('src');
	video.load();

	if (Module.webMovie.objectUrl) {
		URL.revokeObjectURL(Module.webMovie.objectUrl);
		Module.webMovie.objectUrl = undefined;
	}

	Module.webMovie.playing = false;
	Module.webMovie.ended = false;
	Module.webMovie.error = "";
});

EM_JS(int, EasyRpgWebMovieIsPlaying, (), {
	if (!Module.webMovie || !Module.webMovie.element) {
		return 0;
	}
	const video = Module.webMovie.element;
	return Module.webMovie.playing && !video.ended ? 1 : 0;
});

EM_JS(int, EasyRpgWebMovieHasError, (), {
	if (!Module.webMovie || !Module.webMovie.error) {
		return 0;
	}
	return 1;
});

EM_JS(int, EasyRpgWebMovieGetNativeWidth, (), {
	if (!Module.webMovie || !Module.webMovie.element) {
		return 0;
	}
	return Module.webMovie.element.videoWidth || 0;
});

EM_JS(int, EasyRpgWebMovieGetNativeHeight, (), {
	if (!Module.webMovie || !Module.webMovie.element) {
		return 0;
	}
	return Module.webMovie.element.videoHeight || 0;
});

WebMoviePlayer::~WebMoviePlayer() {
	Stop();
}

bool WebMoviePlayer::Open(std::string_view path, std::string& error_message) {
	const auto path_string = std::string(path);
	opened = EasyRpgWebMovieOpen(path_string.c_str()) != 0;
	if (!opened) {
		error_message = "browser video playback failed";
	}
	return opened;
}

void WebMoviePlayer::Update(const Rect& dst_rect) {
	if (!opened) {
		return;
	}

	EasyRpgWebMovieUpdate(
		dst_rect.x,
		dst_rect.y,
		std::max(dst_rect.width, 1),
		std::max(dst_rect.height, 1));
}

void WebMoviePlayer::Stop() {
	if (!opened) {
		return;
	}

	EasyRpgWebMovieStop();
	opened = false;
}

bool WebMoviePlayer::IsPlaying() const {
	return opened && EasyRpgWebMovieIsPlaying() != 0;
}

bool WebMoviePlayer::HasError() const {
	return opened && EasyRpgWebMovieHasError() != 0;
}

int WebMoviePlayer::GetNativeWidth() const {
	return EasyRpgWebMovieGetNativeWidth();
}

int WebMoviePlayer::GetNativeHeight() const {
	return EasyRpgWebMovieGetNativeHeight();
}
