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

#include "scene_history.h"
#include "input.h"
#include "player.h"
#include "transition.h"
#include "game_message.h"
#include "game_system.h"
#include "cache.h"

Scene_History::Scene_History() {
	Scene::type = Scene::History;
}

void Scene_History::Start() {
	// Play enter sound effect
	Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Decision));

	// Create history entry windows (Scene_File style)
	CreateHistoryWindows();

	// Initialize to show newest messages first (start from bottom)
	const auto& entries = Game_Message::GetMessageHistory().GetEntries();
	if (!entries.empty()) {
		// Calculate top_index to show the last few entries
		// Use fixed visible count (3 windows visible on screen)
		const int visible_count = 3;
		top_index = std::max(0, static_cast<int>(entries.size()) - visible_count);
	}

	// Set initial window positions based on top_index
	const int window_height = 80;
	const int start_y = 0;
	for (int i = 0; i < static_cast<int>(history_windows.size()); ++i) {
		auto* w = history_windows[i].get();
		w->SetY(start_y + (i - top_index) * window_height);
	}

	// Refresh windows with data
	RefreshWindows();

	// Update arrow visibility
	UpdateArrows();
}

void Scene_History::vUpdate() {
	// Update arrow animation frame
	arrow_frame = (arrow_frame + 1) % (arrow_animation_frames * 2);

	UpdateArrows();

	// Update all history windows
	for (auto& hw : history_windows) {
		hw->Update();
	}

	// If windows are moving, don't process input
	if (IsWindowMoving()) {
		return;
	}

	// Handle close input
	if (Input::IsTriggered(Input::CANCEL) || Input::IsTriggered(Input::HISTORY_MENU)) {
		// Play cancel sound effect
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cancel));
		Scene::Pop();
		return;
	}

	const auto& entries = Game_Message::GetMessageHistory().GetEntries();
	if (entries.empty()) return;

	int old_top_index = top_index;
	const int visible_count = 3;
	int max_top_index = std::max(0, static_cast<int>(entries.size()) - visible_count);

	// Handle scrolling input - directly modify top_index
	if (Input::IsRepeated(Input::UP)) {
		if (top_index > 0) {
			top_index--;
			// Play cursor sound effect
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cursor));
		}
	}
	if (Input::IsRepeated(Input::DOWN)) {
		if (top_index < max_top_index) {
			top_index++;
			// Play cursor sound effect
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cursor));
		}
	}

	// If scroll position changed, trigger animation and refresh
	if (old_top_index != top_index) {
		// Calculate movement distance and trigger animation
		// 80 is the window height, 7 is the animation duration in frames
		MoveHistoryWindows((old_top_index - top_index) * 80, 7);
		RefreshWindows();
	}
}

void Scene_History::MoveHistoryWindows(int dy, int dt) {
	// Smooth scrolling animation (similar to Scene_File::MoveFileWindows)
	for (auto& hw : history_windows) {
		hw->InitMovement(hw->GetX(), hw->GetY(), hw->GetX(), hw->GetY() + dy, dt);
	}
}

void Scene_History::UpdateArrows() {
	const auto& entries = Game_Message::GetMessageHistory().GetEntries();
	const int visible_count = 3;

	// Calculate arrow visibility with blinking effect
	bool arrow_visible = (arrow_frame < arrow_animation_frames);

	// Show up arrow if there are entries above the visible area
	bool show_up_arrow = (top_index > 0);
	if (up_arrow) {
		up_arrow->SetVisible(show_up_arrow && arrow_visible);
	}

	// Show down arrow if there are entries below the visible area
	bool show_down_arrow = (top_index + visible_count < static_cast<int>(entries.size()));
	if (down_arrow) {
		down_arrow->SetVisible(show_down_arrow && arrow_visible);
	}
}

bool Scene_History::IsWindowMoving() const {
	for (auto& hw : history_windows) {
		if (hw->IsMovementActive()) {
			return true;
		}
	}
	return false;
}

void Scene_History::TransitionIn(SceneType prev_scene) {
	Transition::instance().InitShow(Transition::TransitionFadeIn, this, 6);
}

void Scene_History::TransitionOut(SceneType next_scene) {
	Transition::instance().InitErase(Transition::TransitionFadeOut, this, 6);
}

std::unique_ptr<Sprite> Scene_History::MakeArrowSprite(bool down) {
	int sprite_width = 8;
	int sprite_height = 8;

	Rect rect = Rect(40, (down ? 16 : sprite_height), 16, sprite_height);
	auto bitmap = Bitmap::Create(*(Cache::System()), rect);
	auto sprite = std::make_unique<Sprite>();
	sprite->SetVisible(false);
	sprite->SetZ(Priority_Window + 2);
	sprite->SetBitmap(bitmap);
	sprite->SetX((Player::screen_width / 2) - sprite_width);
	sprite->SetY(down ? Player::screen_height - sprite_height : 0);  // Up arrow at top
	return sprite;
}

void Scene_History::CreateHistoryWindows() {
	const auto& entries = Game_Message::GetMessageHistory().GetEntries();
	const int window_height = 80;
	const int start_y = 0;

	// Create a window for each history entry (like Scene_File does for save files)
	int num_entries = static_cast<int>(entries.size());
	if (num_entries == 0) {
		num_entries = 1;  // Create at least one window for empty state
	}

	for (int i = 0; i < num_entries; ++i) {
		auto w = std::make_shared<Window_HistoryEntry>(
			Player::menu_offset_x,
			start_y + i * window_height,  // Initial Y position
			Player::screen_width,
			window_height
		);
		w->SetZ(Priority_Window);
		history_windows.push_back(w);
	}

	// Create arrow sprites
	up_arrow = MakeArrowSprite(false);
	down_arrow = MakeArrowSprite(true);
}

void Scene_History::RefreshWindows() {
	const auto& entries = Game_Message::GetMessageHistory().GetEntries();

	for (int i = 0; i < static_cast<int>(history_windows.size()); ++i) {
		auto* w = history_windows[i].get();

		// Populate window with history entry data
		// Note: Window Y position is controlled by InitMovement() animation, not SetY()
		if (i >= 0 && i < static_cast<int>(entries.size())) {
			w->SetHistoryEntry(entries[i]);
			w->Refresh();
			w->SetVisible(true);
		} else {
			w->SetVisible(false);
		}
	}
}
