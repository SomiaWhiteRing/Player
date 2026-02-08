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

#ifndef EP_SCENE_HISTORY_H
#define EP_SCENE_HISTORY_H

#include "scene.h"
#include "window_history_entry.h"
#include "sprite.h"
#include <memory>
#include <vector>

/**
 * Scene_History class.
 * Displays message history in Scene_File-style scrollable window frames.
 */
class Scene_History : public Scene {
public:
	/**
	 * Constructor.
	 */
	Scene_History();

	void Start() override;
	void vUpdate() override;
	void TransitionIn(SceneType prev_scene) override;
	void TransitionOut(SceneType next_scene) override;

private:
	/**
	 * Creates an arrow sprite.
	 *
	 * @param down true for down arrow, false for up arrow
	 * @return arrow sprite
	 */
	std::unique_ptr<Sprite> MakeArrowSprite(bool down);

	/**
	 * Creates the history entry windows.
	 */
	void CreateHistoryWindows();

	/**
	 * Refreshes all history windows with current data.
	 */
	void RefreshWindows();

	/**
	 * Moves history windows for scrolling animation.
	 *
	 * @param dy vertical movement amount
	 * @param dt animation duration
	 */
	void MoveHistoryWindows(int dy, int dt);

	/**
	 * Updates scroll arrow visibility.
	 */
	void UpdateArrows();

	/**
	 * Checks if any history window is currently moving.
	 *
	 * @return true if any window is moving
	 */
	bool IsWindowMoving() const;

	/** Index of the topmost visible history entry */
	int top_index = 0;

	/** Multiple history entry windows (Scene_File style) */
	std::vector<std::shared_ptr<Window_HistoryEntry>> history_windows;

	/** Scroll indicator arrows */
	std::unique_ptr<Sprite> up_arrow;
	std::unique_ptr<Sprite> down_arrow;

	/** Arrow animation frame counter */
	int arrow_frame = 0;
	static constexpr int arrow_animation_frames = 20;
};

#endif
