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

#include <cstdlib>
#include <fstream>
#include <map>

#ifdef EMSCRIPTEN
#  include <emscripten.h>
#endif

#include "async_handler.h"
#include "cache.h"
#include "filefinder.h"
#include "memory_management.h"
#include "output.h"
#include "player.h"
#include "main_data.h"
#include "utils.h"
#include "transition.h"
#include "rand.h"

// When this option is enabled async requests are randomly delayed.
// This allows testing some aspects of async file fetching locally.
//#define EP_DEBUG_SIMULATE_ASYNC

namespace {
	std::unordered_map<std::string, FileRequestAsync> async_requests;
	int next_id = 0;

	FileRequestAsync* GetRequest(const std::string& path) {
		auto it = async_requests.find(path);

		if (it != async_requests.end()) {
			return &(it->second);
		}
		return nullptr;
	}

	FileRequestAsync* RegisterRequest(std::string path, std::string directory, std::string file)
	{
		auto req = FileRequestAsync(path, std::move(directory), std::move(file));
		auto p = async_requests.emplace(std::make_pair(std::move(path), std::move(req)));
		return &p.first->second;
	}

	FileRequestBinding CreatePending() {
		return std::make_shared<int>(next_id++);
	}

}

void AsyncHandler::ClearRequests() {
	auto it = async_requests.begin();
	while (it != async_requests.end()) {
		if (it->second.IsReady()) {
			it = async_requests.erase(it);
		} else {
			++it;
		}
	}
	async_requests.clear();
}

FileRequestAsync* AsyncHandler::RequestFile(std::string_view folder_name, std::string_view file_name) {
	auto path = FileFinder::MakePath(folder_name, file_name);

	auto* request = GetRequest(path);

	if (request) {
		return request;
	}

	//Output::Debug("Waiting for {}", path);

	return RegisterRequest(std::move(path), std::string(folder_name), std::string(file_name));
}

FileRequestAsync* AsyncHandler::RequestFile(std::string_view file_name) {
	return RequestFile(".", file_name);
}

bool AsyncHandler::IsFilePending(bool important, bool graphic) {
	for (auto& ap: async_requests) {
		FileRequestAsync& request = ap.second;

#ifdef EP_DEBUG_SIMULATE_ASYNC
		request.UpdateProgress();
#endif

		if (!request.IsReady()
				&& (!important || request.IsImportantFile())
				&& (!graphic || request.IsGraphicFile())
				) {
			return true;
		}
	}

	return false;
}

void AsyncHandler::SaveFilesystem() {
#ifdef EMSCRIPTEN
	// Save changed file system
	EM_ASM({
		Module.syncSaves();
	});
#endif
}

bool AsyncHandler::IsImportantFilePending() {
	return IsFilePending(true, false);
}

bool AsyncHandler::IsGraphicFilePending() {
	return IsFilePending(false, true);
}

FileRequestAsync::FileRequestAsync(std::string path, std::string directory, std::string file) :
	directory(std::move(directory)),
	file(std::move(file)),
	path(std::move(path)),
	state(State_WaitForStart)
{ }

void FileRequestAsync::SetGraphicFile(bool graphic) {
	this->graphic = graphic;
	// We need this flag in order to prevent show screen transitions
	// from starting util all graphical assets are loaded.
	// Also, the screen is erased, so you can't see any delays :)
	if (Transition::instance().IsErasedNotActive()) {
		SetImportantFile(true);
	}
}

void FileRequestAsync::Start() {
	if (file == CACHE_DEFAULT_BITMAP) {
		// Embedded asset -> Fire immediately
		DownloadDone(true);
		return;
	}

	if (state == State_Pending) {
		return;
	}

	if (IsReady()) {
		// Fire immediately
		DownloadDone(true);
		return;
	}

	state = State_Pending;

// The site mounts every installed file before starting the engine.
#ifndef EP_DEBUG_SIMULATE_ASYNC
	DownloadDone(true);
#endif
}

void FileRequestAsync::UpdateProgress() {
#ifndef EMSCRIPTEN
	// Fake download for testing event handlers

	if (!IsReady() && Rand::ChanceOf(1, 100)) {
		DownloadDone(true);
	}
#endif
}

FileRequestBinding FileRequestAsync::Bind(void(*func)(FileRequestResult*)) {
	FileRequestBinding pending = CreatePending();

	listeners.emplace_back(FileRequestBindingWeak(pending), func);

	return pending;
}

FileRequestBinding FileRequestAsync::Bind(std::function<void(FileRequestResult*)> func) {
	FileRequestBinding pending = CreatePending();

	listeners.emplace_back(FileRequestBindingWeak(pending), func);

	return pending;
}

void FileRequestAsync::CallListeners(bool success) {
	FileRequestResult result { directory, file, -1, success };

	for (auto& listener : listeners) {
		if (!listener.first.expired()) {
			result.request_id = *listener.first.lock();
			(listener.second)(&result);
		} else {
			Output::Debug("Request cancelled: {}", GetPath());
		}
	}

	listeners.clear();
}

void FileRequestAsync::DownloadDone(bool success) {
	if (IsReady()) {
		// Change to real success state when already finished before
		success = state == State_DoneSuccess;
	}

	if (success) {


		state = State_DoneSuccess;

		CallListeners(true);
	}
	else {
		state = State_DoneFailure;

		CallListeners(false);
	}
}
