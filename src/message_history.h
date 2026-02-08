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

#ifndef EP_MESSAGE_HISTORY_H
#define EP_MESSAGE_HISTORY_H

#include <deque>
#include <cstdint>
#include <string>
#include <vector>

/**
 * Represents a single message history entry.
 * Stores complete message state for re-rendering (Method B).
 */
struct MessageHistoryEntry {
	/** Complete message text (including control characters) */
	std::string text;

	/** Face graphic filename */
	std::string face_name;

	/** Face index in the face graphic */
	int face_index = -1;

	/** Whether the face is flipped/mirrored */
	bool face_flipped = false;

	/** Whether the face is positioned on the right side */
	bool face_right_position = false;

	/** Window position: 0=top, 1=middle, 2=bottom */
	int window_position = 2;

	/** Whether the message window was transparent */
	bool transparent = false;

	/** Whether the window position is fixed */
	bool position_fixed = false;

	/** System graphic filename (window skin) - CRITICAL for re-rendering */
	std::string system_name;

	/** Message stretch style */
	int message_stretch = 0;

	/** Choice options (if this was a choice message) */
	std::vector<std::string> choices;

	/** Index of the choice the player selected (-1 if not a choice or not yet selected) */
	int selected_choice = -1;

	/** Timestamp when the message was displayed (frame counter) */
	uint32_t timestamp = 0;
};

/**
 * Manages the message history system.
 * Stores up to MAX_ENTRIES messages in a FIFO queue.
 */
class MessageHistory {
public:
	/** Maximum number of history entries to store */
	static constexpr int MAX_ENTRIES = 100;

	/**
	 * Adds a new entry to the history.
	 * If the history is full, removes the oldest entry.
	 *
	 * @param entry The message entry to add
	 */
	void AddEntry(MessageHistoryEntry entry);

	/**
	 * Gets all history entries.
	 * Entries are ordered from oldest to newest.
	 *
	 * @return Reference to the deque of history entries
	 */
	const std::deque<MessageHistoryEntry>& GetEntries() const;

	/**
	 * Clears all history entries.
	 */
	void Clear();

	/**
	 * Gets the number of entries currently stored.
	 *
	 * @return Number of entries
	 */
	size_t GetCount() const;

	/**
	 * Checks if the history is empty.
	 *
	 * @return true if no entries are stored
	 */
	bool IsEmpty() const;

private:
	/** Storage for history entries (oldest first, newest last) */
	std::deque<MessageHistoryEntry> entries;
};

#endif
