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

#include <cassert>

#include "scene_name.h"
#include "game_actors.h"
#include "game_system.h"
#include "input.h"
#include "player.h"
#include "output.h"

namespace {
	constexpr int kMarginX = 32;
	constexpr int kMarginY = 8;
	constexpr int kWindowFaceWidth = 64;
	constexpr int kWindowFaceHeight = 64;
	constexpr int kWindowNameWidth = 192;
	constexpr int kWindowNameHeight = 32;
	constexpr int kWindowKeyboardWidth = 256;
	constexpr int kWindowKeyboardHeight = 160;
	constexpr int kWindowChoiceVisibleItems = 9;
	const char kManualInputLabel[] = "\xE4\xB8\xBB\xE5\x8A\xA8\xE8\xBE\x93\xE5\x85\xA5";
}

Scene_Name::Scene_Name(Game_Actor& actor, int charset, bool use_default_name, std::vector<NameInputCandidate> candidates)
	: layout_index(charset), use_default_name(use_default_name), actor(actor), candidates(std::move(candidates))
{
	Scene::type = Scene::Name;
}

void Scene_Name::Start() {
	CreateFaceWindow();
	CreateNameWindow();
	SetupLayouts();

	if (candidates.empty()) {
		EnterKeyboardInputMode();
	} else {
		EnterChoiceMode();
	}
}

void Scene_Name::CreateFaceWindow() {
	face_window.reset(new Window_Face(Player::menu_offset_x + kMarginX, Player::menu_offset_y + kMarginY, kWindowFaceWidth, kWindowFaceHeight));
	face_window->Set(actor);
	face_window->Refresh();
}

void Scene_Name::CreateNameWindow() {
	name_window.reset(new Window_Name(Player::menu_offset_x + kWindowFaceWidth + kMarginX, Player::menu_offset_y + kMarginY + 32, kWindowNameWidth, kWindowNameHeight));
	name_window->Set(use_default_name ? ToString(actor.GetName()) : "");
}

void Scene_Name::SetupLayouts() {
	layouts.clear();
	keyboard_done = Window_Keyboard::DONE;
	// Japanese pages
	if (Player::IsCP932()) {
		layouts.push_back(Window_Keyboard::Hiragana);
		layouts.push_back(Window_Keyboard::Katakana);
		keyboard_done = Window_Keyboard::DONE_JP;
	// Korean pages
	} else if (Player::IsCP949()) {
		layouts.push_back(Window_Keyboard::Hangul1);
		layouts.push_back(Window_Keyboard::Hangul2);
		keyboard_done = Window_Keyboard::DONE_KO;
	// Simp. Chinese pages
	} else if (Player::IsCP936()) {
		layouts.push_back(Window_Keyboard::ZhCn1);
		layouts.push_back(Window_Keyboard::ZhCn2);
		keyboard_done = Window_Keyboard::DONE_ZH_CN;
	// Trad. Chinese pages
	} else if (Player::IsBig5()) {
		layouts.push_back(Window_Keyboard::ZhTw1);
		layouts.push_back(Window_Keyboard::ZhTw2);
		keyboard_done = Window_Keyboard::DONE_ZH_TW;
	// Cyrillic page (we assume it's Russian since we have no way to detect Serbian etc.)
	} else if (Player::IsCP1251()) {
		layouts.push_back(Window_Keyboard::RuCyrl);
		keyboard_done = Window_Keyboard::DONE_RU;
	}

	// Letter and symbol pages are used everywhere
	layouts.push_back(Window_Keyboard::Letter);
	layouts.push_back(Window_Keyboard::Symbol);
}

void Scene_Name::CreateKeyboardWindow() {
	if (kbd_window) {
		return;
	}

	if (layout_index < 0 || layout_index >= static_cast<int>(layouts.size())) {
		layout_index = 0;
	}

	kbd_window.reset(new Window_Keyboard(
		Player::menu_offset_x + kMarginX,
		Player::menu_offset_y + kWindowFaceHeight + kMarginY,
		kWindowKeyboardWidth,
		kWindowKeyboardHeight,
		keyboard_done));

	auto next_index = layout_index + 1;
	if (next_index >= static_cast<int>(layouts.size())) {
		next_index = 0;
	}
	kbd_window->SetMode(layouts[layout_index], layouts[next_index]);

	kbd_window->Refresh();
	kbd_window->UpdateCursorRect();
}

std::vector<std::string> Scene_Name::GetChoiceLabels() const {
	std::vector<std::string> labels;
	labels.reserve(candidates.size() + 1);

	for (const auto& candidate: candidates) {
		labels.push_back(candidate.label);
	}

	labels.emplace_back(kManualInputLabel);
	return labels;
}

void Scene_Name::CreateChoiceWindow() {
	choice_window.reset(new Window_Command(GetChoiceLabels(), kWindowKeyboardWidth, kWindowChoiceVisibleItems));
	choice_window->SetX(Player::menu_offset_x + kMarginX);
	choice_window->SetY(Player::menu_offset_y + kWindowFaceHeight + kMarginY);
	choice_window->UpdateCursorRect();
}

void Scene_Name::EnterChoiceMode() {
	mode = Mode::Choice;
	kbd_window.reset();
	name_window->SetActive(false);
	CreateChoiceWindow();
	choice_window->SetActive(true);
}

void Scene_Name::EnterKeyboardInputMode() {
	mode = Mode::Input;
	choice_window.reset();
	name_window->SetActive(true);
	CreateKeyboardWindow();
	kbd_window->SetActive(true);
}

void Scene_Name::vUpdate() {
	if (mode == Mode::Choice) {
		choice_window->Update();

		if (Input::IsTriggered(Input::CANCEL)) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Buzzer));
		} else if (Input::IsTriggered(Input::DECISION)) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Decision));
			auto index = choice_window->GetIndex();

			if (index == static_cast<int>(candidates.size())) {
				EnterKeyboardInputMode();
			} else if (index >= 0 && index < static_cast<int>(candidates.size())) {
				actor.SetName(candidates[index].value);
				Scene::Pop();
			}
		}
		return;
	}

	kbd_window->Update();
	name_window->Update();

	if (Input::IsTriggered(Input::CANCEL)) {
		if (name_window->Get().size() > 0) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cancel));
			name_window->Erase();
		} else {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Buzzer));
		}
	} else if (Input::IsTriggered(Input::DECISION)) {
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Decision));
		std::string const& s = kbd_window->GetSelected();

		assert(!s.empty());

		if (s == Window_Keyboard::DONE) {
			if (name_window->Get().empty()) {
				name_window->Set(ToString(actor.GetName()));
				name_window->Refresh();
			} else {
				actor.SetName(name_window->Get());
				Scene::Pop();
			}
		} else if (s == Window_Keyboard::NEXT_PAGE) {
			++layout_index;
			if (layout_index >= static_cast<int>(layouts.size())) {
				layout_index = 0;
			}

			auto next_index = layout_index + 1;
			if (next_index >= static_cast<int>(layouts.size())) {
				next_index = 0;
			}
			kbd_window->SetMode(layouts[layout_index], layouts[next_index]);
		} else if (s == Window_Keyboard::SPACE) {
			name_window->Append(" ");
		} else {
			name_window->Append(s);
		}
	}
}
