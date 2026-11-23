#pragma once

#include <cstddef>
#include <string>

#include "core/log.hpp"
#include "editor/editor.hpp"

namespace engine::editor
{
	// Which levels the Console shows (all by default).
	struct ConsoleFilter
	{
		bool debug = true;
		bool info = true;
		bool warning = true;
		bool error = true;
		std::string text; // shown only if the message contains this, ignoring case; empty matches everything
	};

	// Adds the Console panel: the messages logged through core::Log (see core::LogHistory), newest at the bottom,
	// coloured by level, with a button per level, a text filter, Clear, Copy and auto-scroll. Turns the log history
	// on (core::SetLogHistoryCapacity). The panel starts closed.
	void AddConsolePanel(Editor& editor, std::size_t capacity = 2000);

	// True if the record passes the filter (its level is on and its message contains the text).
	[[nodiscard]] bool ConsoleMatches(const core::LogRecord& record, const ConsoleFilter& filter);

	// "16:09:27.123 [ERROR] message": what Copy puts on the clipboard, one line per record.
	[[nodiscard]] std::string ConsoleLine(const core::LogRecord& record);
} // namespace engine::editor
