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

#include "window_pinyin.h"

#include <algorithm>
#include <utility>

#include "bitmap.h"
#include "font.h"

Window_Pinyin::Window_Pinyin(int ix, int iy, int iwidth, int iheight)
	: Window_Base(ix, iy, iwidth, iheight) {
	SetContents(Bitmap::Create(width - 16, height - 16));
	Refresh();
}

void Window_Pinyin::SetQuery(std::string nquery) {
	query = std::move(nquery);
	Refresh();
}

void Window_Pinyin::SetCandidates(std::vector<std::string> ncandidates) {
	candidates = std::move(ncandidates);
	index = 0;
	Refresh();
}

void Window_Pinyin::Clear() {
	query.clear();
	candidates.clear();
	index = 0;
	Refresh();
}

void Window_Pinyin::MovePage(int delta) {
	if (candidates.empty()) {
		return;
	}

	const auto page_count = GetPageCount();
	auto page = index / kVisibleCandidates;
	page = (page + delta) % page_count;
	if (page < 0) {
		page += page_count;
	}
	index = page * kVisibleCandidates;
	Refresh();
}

void Window_Pinyin::SetSelection(int nindex) {
	if (nindex < 0 || nindex >= static_cast<int>(candidates.size())) {
		return;
	}
	index = nindex;
	Refresh();
}

bool Window_Pinyin::SetVisibleSelection(int slot) {
	if (slot < 0 || slot >= kVisibleCandidates) {
		return false;
	}

	const auto candidate_index = GetFirstVisibleIndex() + slot;
	if (candidate_index >= static_cast<int>(candidates.size())) {
		return false;
	}

	SetSelection(candidate_index);
	return true;
}

int Window_Pinyin::GetSelection() const {
	return index;
}

bool Window_Pinyin::HasSelection() const {
	return !candidates.empty();
}

const std::string& Window_Pinyin::GetSelectedCandidate() const {
	static const std::string empty;
	if (!HasSelection()) {
		return empty;
	}
	return candidates[index];
}

int Window_Pinyin::GetFirstVisibleIndex() const {
	return (index / kVisibleCandidates) * kVisibleCandidates;
}

int Window_Pinyin::GetPageCount() const {
	if (candidates.empty()) {
		return 0;
	}
	return (static_cast<int>(candidates.size()) + kVisibleCandidates - 1) / kVisibleCandidates;
}

void Window_Pinyin::Refresh() {
	contents->Clear();

	std::string line = "拼音:";
	line += query.empty() ? "_" : query;
	if (!candidates.empty()) {
		line += " ";
		line += std::to_string(index / kVisibleCandidates + 1);
		line += "/";
		line += std::to_string(GetPageCount());
	}
	contents->TextDraw(0, 0, Font::ColorDefault, line);

	int x = Text::GetSize(*Font::Default(), line).width + 8;
	const int first = GetFirstVisibleIndex();
	const int count = std::min<int>(kVisibleCandidates, static_cast<int>(candidates.size()) - first);
	for (int i = 0; i < count; ++i) {
		const int candidate_index = first + i;
		if (candidate_index >= static_cast<int>(candidates.size())) {
			break;
		}

		std::string item;
		item += static_cast<char>('1' + i);
		item += '.';
		item.append(candidates[candidate_index]);

		const auto color = candidate_index == index ? Font::ColorDefault : Font::ColorDisabled;
		contents->TextDraw(x, 0, color, item);
		x += Text::GetSize(*Font::Default(), item).width + 6;
	}
}
