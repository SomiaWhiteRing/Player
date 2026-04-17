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

#include "platform/windows/movie_player_dshow.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include <dshow.h>

#include "baseui.h"
#include "utils.h"

namespace {
	template <typename T>
	void SafeRelease(T*& ptr) {
		if (ptr) {
			ptr->Release();
			ptr = nullptr;
		}
	}

	std::string HrToString(HRESULT hr) {
		std::ostringstream os;
		os << "DirectShow error 0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
		return os.str();
	}
}

struct DirectShowMoviePlayer::Impl {
	IGraphBuilder* graph = nullptr;
	IMediaControl* media_control = nullptr;
	IMediaEventEx* media_event = nullptr;
	IVideoWindow* video_window = nullptr;
	IBasicVideo* basic_video = nullptr;
	HWND parent_window = nullptr;
	Rect last_rect = {};
	int native_width = 0;
	int native_height = 0;
	bool com_available = false;
	bool com_initialized = false;
	bool playing = false;
};

DirectShowMoviePlayer::DirectShowMoviePlayer() : impl(std::make_unique<Impl>()) {
	const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (SUCCEEDED(hr)) {
		impl->com_available = true;
		impl->com_initialized = true;
	} else if (hr == RPC_E_CHANGED_MODE) {
		// Another subsystem already initialized COM on this thread with a different apartment model.
		// DirectShow can still use the existing COM apartment.
		impl->com_available = true;
	}
}

DirectShowMoviePlayer::~DirectShowMoviePlayer() {
	ResetGraph();
	if (impl->com_initialized) {
		CoUninitialize();
	}
}

bool DirectShowMoviePlayer::CheckWindowAvailable(std::string& error_message) {
	if (!DisplayUi) {
		error_message = "video output is unavailable";
		return false;
	}

	impl->parent_window = static_cast<HWND>(DisplayUi->GetNativeWindowHandle());
	if (!impl->parent_window) {
		error_message = "native window handle is unavailable";
		return false;
	}

	return true;
}

bool DirectShowMoviePlayer::Open(std::string_view path, std::string& error_message) {
	ResetGraph();

	if (!impl->com_available) {
		error_message = "COM initialization failed";
		return false;
	}

	if (!CheckWindowAvailable(error_message)) {
		return false;
	}

	const auto wide_path = Utils::ToWideString(path);

	HRESULT hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
		IID_IGraphBuilder, reinterpret_cast<void**>(&impl->graph));
	if (FAILED(hr)) {
		error_message = HrToString(hr);
		return false;
	}

	hr = impl->graph->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&impl->media_control));
	if (FAILED(hr)) {
		error_message = HrToString(hr);
		ResetGraph();
		return false;
	}

	hr = impl->graph->QueryInterface(IID_IMediaEventEx, reinterpret_cast<void**>(&impl->media_event));
	if (FAILED(hr)) {
		error_message = HrToString(hr);
		ResetGraph();
		return false;
	}

	impl->graph->QueryInterface(IID_IVideoWindow, reinterpret_cast<void**>(&impl->video_window));
	impl->graph->QueryInterface(IID_IBasicVideo, reinterpret_cast<void**>(&impl->basic_video));

	hr = impl->graph->RenderFile(wide_path.c_str(), nullptr);
	if (FAILED(hr)) {
		error_message = HrToString(hr);
		ResetGraph();
		return false;
	}

	if (impl->basic_video) {
		long width = 0;
		long height = 0;
		if (SUCCEEDED(impl->basic_video->get_VideoWidth(&width))) {
			impl->native_width = std::max<long>(width, 0);
		}
		if (SUCCEEDED(impl->basic_video->get_VideoHeight(&height))) {
			impl->native_height = std::max<long>(height, 0);
		}
	}

	if (impl->video_window) {
		impl->video_window->put_AutoShow(OAFALSE);
		impl->video_window->put_Owner(reinterpret_cast<OAHWND>(impl->parent_window));
		impl->video_window->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
		impl->video_window->put_MessageDrain(reinterpret_cast<OAHWND>(impl->parent_window));
	}

	hr = impl->media_control->Run();
	if (FAILED(hr)) {
		error_message = HrToString(hr);
		ResetGraph();
		return false;
	}

	impl->playing = true;
	ApplyRect(impl->last_rect);
	return true;
}

void DirectShowMoviePlayer::PollEvents() {
	if (!impl->media_event) {
		return;
	}

	long code = 0;
	LONG_PTR param1 = 0;
	LONG_PTR param2 = 0;

	while (SUCCEEDED(impl->media_event->GetEvent(&code, &param1, &param2, 0))) {
		impl->media_event->FreeEventParams(code, param1, param2);
		if (code == EC_COMPLETE || code == EC_USERABORT || code == EC_ERRORABORT) {
			impl->playing = false;
		}
	}
}

void DirectShowMoviePlayer::ApplyRect(const Rect& dst_rect) {
	impl->last_rect = dst_rect;

	if (!impl->video_window) {
		return;
	}

	if (dst_rect.width <= 0 || dst_rect.height <= 0) {
		impl->video_window->put_Visible(OAFALSE);
		return;
	}

	impl->video_window->SetWindowPosition(dst_rect.x, dst_rect.y, dst_rect.width, dst_rect.height);
	impl->video_window->put_Visible(OATRUE);
}

void DirectShowMoviePlayer::Update(const Rect& dst_rect) {
	if (!impl->playing) {
		return;
	}

	PollEvents();
	if (!impl->playing) {
		ResetGraph();
		return;
	}

	if (DisplayUi) {
		auto current_parent = static_cast<HWND>(DisplayUi->GetNativeWindowHandle());
		if (current_parent && current_parent != impl->parent_window && impl->video_window) {
			impl->parent_window = current_parent;
			impl->video_window->put_Owner(reinterpret_cast<OAHWND>(impl->parent_window));
			impl->video_window->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
		}
	}

	ApplyRect(dst_rect);
}

void DirectShowMoviePlayer::ResetGraph() {
	if (impl->media_control) {
		impl->media_control->Stop();
	}

	if (impl->video_window) {
		impl->video_window->put_Visible(OAFALSE);
		impl->video_window->put_MessageDrain(0);
		impl->video_window->put_Owner(0);
	}

	SafeRelease(impl->basic_video);
	SafeRelease(impl->video_window);
	SafeRelease(impl->media_event);
	SafeRelease(impl->media_control);
	SafeRelease(impl->graph);

	impl->parent_window = nullptr;
	impl->last_rect = {};
	impl->native_width = 0;
	impl->native_height = 0;
	impl->playing = false;
}

void DirectShowMoviePlayer::Stop() {
	ResetGraph();
}

bool DirectShowMoviePlayer::IsPlaying() const {
	return impl->playing;
}

int DirectShowMoviePlayer::GetNativeWidth() const {
	return impl->native_width;
}

int DirectShowMoviePlayer::GetNativeHeight() const {
	return impl->native_height;
}
