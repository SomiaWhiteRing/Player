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

#ifndef EP_MOVIE_PLAYER_DSHOW_H
#define EP_MOVIE_PLAYER_DSHOW_H

#include "movie_player.h"

class DirectShowMoviePlayer final : public MoviePlayer {
public:
	DirectShowMoviePlayer();
	~DirectShowMoviePlayer() override;

	bool Open(std::string_view path, std::string& error_message) override;
	void Update(const Rect& dst_rect) override;
	void Stop() override;
	bool IsPlaying() const override;
	int GetNativeWidth() const override;
	int GetNativeHeight() const override;

private:
	void ResetGraph();
	void ApplyRect(const Rect& dst_rect);
	void PollEvents();
	bool CheckWindowAvailable(std::string& error_message);

	struct Impl;
	std::unique_ptr<Impl> impl;
};

#endif
