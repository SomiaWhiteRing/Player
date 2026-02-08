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

#include "window_history_entry.h"
#include "cache.h"
#include "bitmap.h"
#include "font.h"
#include "player.h"
#include "utils.h"
#include "text.h"
#include "game_message.h"
#include "compiler.h"

Window_HistoryEntry::Window_HistoryEntry(int ix, int iy, int iwidth, int iheight) :
	Window_Base(ix, iy, iwidth, iheight) {
	SetContents(Bitmap::Create(iwidth - 16, iheight - 16));
}

void Window_HistoryEntry::SetHistoryEntry(const MessageHistoryEntry& new_entry) {
	entry = new_entry;

	// Load the System graphic specified in the entry
	if (!entry.system_name.empty()) {
		system_bitmap = Cache::System(entry.system_name);
	}

	Refresh();
}

void Window_HistoryEntry::Refresh() {
	if (!contents) {
		return;
	}

	contents->Clear();

	// Draw face graphic if present
	DrawFace();

	// Draw message text
	DrawMessageText();
}

void Window_HistoryEntry::DrawFace() {
	// Check if face is present
	if (entry.face_name.empty() || entry.face_index < 0) {
		return;
	}

	// Load face graphic
	BitmapRef face = Cache::Faceset(entry.face_name);
	if (!face) {
		return;
	}

	// Calculate source rect (faces are 48x48, arranged in 4 columns)
	int face_x = (entry.face_index % 4) * 48;
	int face_y = (entry.face_index / 4) * 48;
	Rect src_rect(face_x, face_y, 48, 48);

	// Calculate destination position
	int dest_x = entry.face_right_position ? (contents->GetWidth() - 48 - 8) : 8;
	int dest_y = 8;

	// Draw face (handle flipping if needed)
	if (entry.face_flipped) {
		contents->FlipBlit(dest_x, dest_y, *face, src_rect, true, false, Opacity::Opaque());
	} else {
		contents->Blit(dest_x, dest_y, *face, src_rect, 255);
	}
}

void Window_HistoryEntry::DrawIcon(int icon_id, int x, int y) {
	// Load System2 graphic which contains icons
	BitmapRef system2 = Cache::System2();
	if (!system2) {
		return;
	}

	// Icons are 16x16, located in System2 graphic
	// Calculate source rect based on icon_id
	int icons_per_row = system2->GetWidth() / 16;
	int src_x = (icon_id % icons_per_row) * 16;
	int src_y = (icon_id / icons_per_row) * 16;

	Rect src_rect(src_x, src_y, 16, 16);
	contents->Blit(x, y, *system2, src_rect, 255);
}

void Window_HistoryEntry::DrawMessageText() {
	// Calculate starting X position (account for face graphic)
	// Match Window_Message behavior: LeftMargin=8, FaceSize=48, RightFaceMargin=16
	int text_x = 0;  // Default: no face or face on right
	if (!entry.face_name.empty() && entry.face_index >= 0) {
		if (!entry.face_right_position) {
			// Face on left: text_x = LeftMargin + FaceSize + RightFaceMargin = 8 + 48 + 16 = 72
			text_x = 72;
		}
	}

	int text_y = 2;
	int line_height = 16;

	// Split text by newlines and draw each line
	std::string remaining_text = entry.text;
	size_t pos = 0;

	while (pos < remaining_text.length()) {
		// Find next newline
		size_t newline_pos = remaining_text.find('\n', pos);
		if (newline_pos == std::string::npos) {
			newline_pos = remaining_text.length();
		}

		// Extract line
		std::string line = remaining_text.substr(pos, newline_pos - pos);

		// Draw line with formatting control character processing
		DrawLineWithFormatting(text_x, text_y, line);

		// Move to next line
		text_y += line_height;
		pos = newline_pos + 1;
	}
}

void Window_HistoryEntry::DrawLineWithFormatting(int x, int y, const std::string& line) {
	int current_x = x;
	int current_color = Font::ColorDefault;

	auto system = Cache::SystemOrBlack();
	auto font = Font::Default();

	const char* text_index = line.data();
	const char* end = line.data() + line.size();

	std::vector<Font::ShapeRet> shape_ret;

	while (text_index < end) {
		// Handle shaped glyphs first
		if (!shape_ret.empty()) {
			auto rect = font->Render(*contents, current_x, y, *system, current_color, shape_ret[0]);
			current_x += rect.x;
			shape_ret.erase(shape_ret.begin());
			continue;
		}

		// Parse next character
		auto tret = Utils::TextNext(text_index, end, Player::escape_char);
		text_index = tret.next;

		if (EP_UNLIKELY(!tret)) {
			continue;
		}

		const auto ch = tret.ch;

		// Handle ExFont characters
		if (tret.is_exfont) {
			auto rect = Text::Draw(*contents, current_x, y, *font, *system, current_color, ch, true);
			current_x += rect.x;
			continue;
		}

		// Skip control characters
		if (Utils::IsControlCharacter(ch)) {
			continue;
		}

		// Handle escape sequences
		if (tret.is_escape && ch != Player::escape_char) {
			switch (ch) {
			case 'c':
			case 'C':
				{
					// Color change
					auto pres = Game_Message::ParseColor(text_index, end, Player::escape_char, true);
					text_index = pres.next;
					current_color = pres.value > 19 ? 0 : pres.value;
				}
				break;
			case 'i':
			case 'I':
				{
					// Icon display
					auto pres = Game_Message::ParseParam('I', 'i', text_index, end, Player::escape_char, true);
					text_index = pres.next;
					DrawIcon(pres.value, current_x, y);
					current_x += 16;
				}
				break;
			case '_':
				// Half-width space
				current_x += Text::GetSize(*font, " ").width / 2;
				break;
			default:
				// Skip unknown escape sequences
				break;
			}
			continue;
		}

		// Handle font shaping for complex scripts
		if (font->CanShape()) {
			std::u32string text32;
			text32.push_back(ch);

			auto text_index_shape = text_index;
			while (true) {
				auto tret_shape = Utils::TextNext(text_index_shape, end, Player::escape_char);

				if (EP_UNLIKELY(!tret_shape)) {
					break;
				}

				auto text_prev_shape = text_index_shape;
				text_index_shape = tret_shape.next;
				auto chs = tret_shape.ch;

				if (text_index_shape == end || tret_shape.is_exfont || tret_shape.is_escape || Utils::IsControlCharacter(chs)) {
					text_index = text_prev_shape;
					break;
				}

				text32.push_back(tret_shape.ch);
			}

			shape_ret = font->Shape(text32);
			continue;
		} else {
			// Draw single glyph without shaping
			auto rect = Text::Draw(*contents, current_x, y, *font, *system, current_color, ch, false);
			current_x += rect.x;
		}
	}
}
