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

#include "recommended_soundfont.h"

#include "audio.h"
#include "audio_midi.h"
#include "filefinder.h"
#include "game_config.h"
#include "output.h"
#include "player.h"

#include <cstddef>
#include <cstdint>
#include <ios>

namespace EmbeddedResources {
	extern const uint8_t recommended_soundfont[];
	extern const std::size_t recommended_soundfont_size;
}

namespace {
	std::string cached_path;
	bool exported = false;

	bool WriteSoundFont(FilesystemView fs, const std::string& filename) {
		if (!fs) {
			return false;
		}

		if (fs.Exists(filename) && fs.GetFilesize(filename) == static_cast<int64_t>(EmbeddedResources::recommended_soundfont_size)) {
			return true;
		}

		auto out = fs.OpenOutputStream(filename, std::ios_base::out | std::ios_base::binary);
		if (!out) {
			return false;
		}

		out.write(reinterpret_cast<const char*>(EmbeddedResources::recommended_soundfont), EmbeddedResources::recommended_soundfont_size);
		const bool ok = out.good();
		out.Close();
		fs.ClearCache();
		return ok;
	}
}

std::string RecommendedSoundFont::GetPath() {
	if (!Player::player_config.extra_recommended_soundfont.Get()) {
		return {};
	}

	if (exported && !cached_path.empty()) {
		return cached_path;
	}

	auto fs = Game_Config::GetSoundfontFilesystem();
	if (WriteSoundFont(fs, kFilename)) {
		cached_path = FileFinder::MakePath(fs.GetFullPath(), kFilename);
		exported = true;
		return cached_path;
	}

	const auto fallback_path = FileFinder::MakePath(".", kFilename);
	if (WriteSoundFont(FileFinder::Root(), fallback_path)) {
		cached_path = fallback_path;
		exported = true;
		return cached_path;
	}

	Output::Warning("Could not export built-in SoundFont");
	return {};
}

void RecommendedSoundFont::Refresh() {
	cached_path.clear();
	exported = false;
	MidiDecoder::ChangeFluidsynthSoundfont(Audio().GetFluidsynthSoundfont());
}
