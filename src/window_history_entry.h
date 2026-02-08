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

#ifndef EP_WINDOW_HISTORY_ENTRY_H
#define EP_WINDOW_HISTORY_ENTRY_H

#include "window_base.h"
#include "message_history.h"
#include "memory_management.h"

/**
 * Window_HistoryEntry class.
 * Displays a single message history entry by re-rendering the message content.
 */
class Window_HistoryEntry : public Window_Base {
public:
	/**
	 * Constructor.
	 *
	 * @param ix window x position
	 * @param iy window y position
	 * @param iwidth window width
	 * @param iheight window height
	 */
	Window_HistoryEntry(int ix, int iy, int iwidth, int iheight);

	/**
	 * Sets the history entry to display.
	 *
	 * @param entry The message history entry
	 */
	void SetHistoryEntry(const MessageHistoryEntry& entry);

	/**
	 * Refreshes the window contents by re-rendering the message.
	 */
	void Refresh();

private:
	/**
	 * Draws the face graphic if present.
	 */
	void DrawFace();

	/**
	 * Draws an icon at the specified position.
	 *
	 * @param icon_id the icon ID to draw
	 * @param x the x position
	 * @param y the y position
	 */
	void DrawIcon(int icon_id, int x, int y);

	/**
	 * Draws the message text with control character processing.
	 */
	void DrawMessageText();

	/**
	 * Draws a single line of text with formatting control characters.
	 *
	 * @param x starting x position
	 * @param y starting y position
	 * @param line the text line to draw
	 */
	void DrawLineWithFormatting(int x, int y, const std::string& line);

	/** The history entry being displayed */
	MessageHistoryEntry entry;

	/** System graphic bitmap for rendering (loaded from entry.system_name) */
	BitmapRef system_bitmap;
};

#endif
