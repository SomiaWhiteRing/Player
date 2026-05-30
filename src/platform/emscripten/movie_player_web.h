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

#ifndef EP_MOVIE_PLAYER_WEB_H
#define EP_MOVIE_PLAYER_WEB_H

#include "movie_player.h"

class WebMoviePlayer final : public MoviePlayer {
public:
	WebMoviePlayer() = default;
	~WebMoviePlayer() override;

	bool Open(std::string_view path, std::string& error_message) override;
	void Update(const Rect& dst_rect) override;
	void Stop() override;
	bool IsPlaying() const override;
	bool HasError() const override;
	int GetNativeWidth() const override;
	int GetNativeHeight() const override;

private:
	bool opened = false;
};

#endif
