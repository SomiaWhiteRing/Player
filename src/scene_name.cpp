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
#include <cctype>
#include <string>

#include "scene_name.h"
#include "game_actors.h"
#include "game_system.h"
#include "input.h"
#include "pinyin_input.h"
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
	constexpr int kWindowPinyinHeight = 32;
	constexpr int kWindowChoiceVisibleItems = 9;
	const char kManualInputLabel[] = "\xE4\xB8\xBB\xE5\x8A\xA8\xE8\xBE\x93\xE5\x85\xA5";

	char GetTriggeredRawPinyinLetter() {
		for (int key = Input::Keys::A; key <= Input::Keys::Z; ++key) {
			if (Input::IsRawKeyTriggered(static_cast<Input::Keys::InputKey>(key))) {
				return static_cast<char>('a' + key - Input::Keys::A);
			}
		}
		return '\0';
	}

	char GetTriggeredRawNumber() {
		for (int key = Input::Keys::N1; key <= Input::Keys::N9; ++key) {
			if (Input::IsRawKeyTriggered(static_cast<Input::Keys::InputKey>(key))) {
				return static_cast<char>('1' + key - Input::Keys::N1);
			}
		}
		return '\0';
	}
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
	if (Player::IsCP936() && Player::player_config.extra_pinyin_input.Get()) {
		layouts.push_back(Window_Keyboard::Pinyin);
	}
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

	pinyin_window.reset(new Window_Pinyin(
		Player::menu_offset_x + kWindowFaceWidth + kMarginX,
		Player::menu_offset_y + kMarginY,
		kWindowNameWidth,
		kWindowPinyinHeight));
	RefreshPinyinWindow();
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
	pinyin_window.reset();
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

bool Scene_Name::IsPinyinMode() const {
	return layout_index >= 0
		&& layout_index < static_cast<int>(layouts.size())
		&& layouts[layout_index] == Window_Keyboard::Pinyin;
}

void Scene_Name::RefreshPinyinWindow() {
	if (!pinyin_window) {
		return;
	}

	pinyin_window->SetVisible(IsPinyinMode());
	pinyin_window->SetActive(IsPinyinMode());
	pinyin_window->SetQuery(pinyin_query);
	pinyin_window->SetCandidates(PinyinInput::Lookup(pinyin_query));
}

void Scene_Name::ResetPinyinInput() {
	pinyin_query.clear();
	RefreshPinyinWindow();
}

void Scene_Name::AppendPinyinLetter(std::string_view key) {
	if (key.empty() || pinyin_query.size() >= PinyinInput::kMaxQueryLength) {
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Buzzer));
		return;
	}

	const auto c = static_cast<char>(std::tolower(static_cast<unsigned char>(key[0])));
	if (!PinyinInput::IsValidInputChar(c)) {
		return;
	}

	pinyin_query.push_back(c);
	RefreshPinyinWindow();
}

bool Scene_Name::CommitPinyinSelection() {
	if (!pinyin_window || !pinyin_window->HasSelection()) {
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Buzzer));
		return false;
	}

	if (!name_window->Append(pinyin_window->GetSelectedCandidate())) {
		return false;
	}
	ResetPinyinInput();
	return true;
}

bool Scene_Name::SelectPinyinCandidateByNumber(std::string_view key) {
	if (!IsPinyinMode() || key.size() != 1 || key[0] < '1' || key[0] > '4') {
		return false;
	}

	if (!pinyin_window->SetVisibleSelection(key[0] - '1')) {
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Buzzer));
		return true;
	}
	return CommitPinyinSelection();
}

bool Scene_Name::HandleRawPinyinInput() {
	if (!IsPinyinMode()) {
		return false;
	}

	if (const auto letter = GetTriggeredRawPinyinLetter()) {
		const std::string key(1, letter);
		AppendPinyinLetter(key);
		return true;
	}

	if (Input::IsRawKeyTriggered(Input::Keys::APOSTROPH)) {
		AppendPinyinLetter("'");
		return true;
	}

	const auto has_query = !pinyin_query.empty();

	if (has_query) {
		if (const auto number = GetTriggeredRawNumber()) {
			const std::string key(1, number);
			return SelectPinyinCandidateByNumber(key);
		}

		if (Input::IsRawKeyTriggered(Input::Keys::BACKSPACE)) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cancel));
			pinyin_query.pop_back();
			RefreshPinyinWindow();
			return true;
		}

		if (Input::IsRawKeyTriggered(Input::Keys::SPACE)) {
			CommitPinyinSelection();
			return true;
		}

		if (pinyin_window && pinyin_window->HasSelection()) {
			if (Input::IsRawKeyTriggered(Input::Keys::PGUP)) {
				pinyin_window->MovePage(-1);
				return true;
			}
			if (Input::IsRawKeyTriggered(Input::Keys::PGDN)) {
				pinyin_window->MovePage(1);
				return true;
			}
		}
	}

	return false;
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

	if (HandleRawPinyinInput()) {
		return;
	}

	if (Input::IsTriggered(Input::CANCEL)) {
		if (IsPinyinMode() && !pinyin_query.empty()) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cancel));
			pinyin_query.pop_back();
			RefreshPinyinWindow();
		} else if (name_window->Get().size() > 0) {
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
			if (IsPinyinMode() && !pinyin_query.empty()) {
				CommitPinyinSelection();
				return;
			}
			if (name_window->Get().empty()) {
				name_window->Set(ToString(actor.GetName()));
				name_window->Refresh();
			} else {
				actor.SetName(name_window->Get());
				Scene::Pop();
			}
		} else if (s == Window_Keyboard::NEXT_PAGE) {
			ResetPinyinInput();
			++layout_index;
			if (layout_index >= static_cast<int>(layouts.size())) {
				layout_index = 0;
			}

			auto next_index = layout_index + 1;
			if (next_index >= static_cast<int>(layouts.size())) {
				next_index = 0;
			}
			kbd_window->SetMode(layouts[layout_index], layouts[next_index]);
			RefreshPinyinWindow();
		} else if (s == Window_Keyboard::SPACE) {
			if (IsPinyinMode() && !pinyin_query.empty()) {
				CommitPinyinSelection();
				return;
			}
			name_window->Append(" ");
		} else if (SelectPinyinCandidateByNumber(s)) {
			return;
		} else if (IsPinyinMode() && s.size() == 1 && PinyinInput::IsValidInputChar(s[0])) {
			AppendPinyinLetter(s);
		} else {
			name_window->Append(s);
		}
	}

	if (IsPinyinMode() && pinyin_window && pinyin_window->HasSelection()) {
		if (Input::IsRepeated(Input::PAGE_UP)) {
			pinyin_window->MovePage(-1);
		} else if (Input::IsRepeated(Input::PAGE_DOWN)) {
			pinyin_window->MovePage(1);
		}
	}
}
