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

#include "message_history.h"
#include <utility>

void MessageHistory::AddEntry(MessageHistoryEntry entry) {
	// Remove oldest entry if at capacity
	if (entries.size() >= MAX_ENTRIES) {
		entries.pop_front();
	}

	// Add new entry to the end (newest)
	entries.push_back(std::move(entry));
}

const std::deque<MessageHistoryEntry>& MessageHistory::GetEntries() const {
	return entries;
}

void MessageHistory::Clear() {
	entries.clear();
}

size_t MessageHistory::GetCount() const {
	return entries.size();
}

bool MessageHistory::IsEmpty() const {
	return entries.empty();
}
