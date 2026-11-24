#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
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

	// What the Console does when a line's source is opened: file as the compiler gave it (an absolute path when the
	// build uses absolute source paths), and its line.
	using SourceOpener = std::function<void(const std::string& file, std::uint32_t line)>;

	// Adds the Console panel: the messages logged through core::Log (see core::LogHistory), newest at the bottom,
	// coloured by level, with a button per level, a text filter, Clear, Copy and auto-scroll. Turns the log history
	// on (core::SetLogHistoryCapacity). Click a line to select it: the strip under the list shows the file and
	// line that logged it, with Open and Copy buttons. `openSource` handles Open; without one the file is handed to the
	// operating system (SDL_OpenURL), which opens it in whatever is set for the file type but cannot go to the line.
	// The panel starts closed.
	void AddConsolePanel(Editor& editor, std::size_t capacity = 2000, SourceOpener openSource = {});

	// "file:line" for the call that logged the record, or empty if the record has no source (SDL's messages).
	[[nodiscard]] std::string ConsoleSource(const core::LogRecord& record);

	// A file: URL for a path (Windows or POSIX), with spaces and other reserved characters escaped.
	[[nodiscard]] std::string FileUrl(const std::string& path);

	// True if the record passes the filter (its level is on and its message contains the text).
	[[nodiscard]] bool ConsoleMatches(const core::LogRecord& record, const ConsoleFilter& filter);

	// "16:09:27.123 [ERROR] message": what Copy puts on the clipboard, one line per record.
	[[nodiscard]] std::string ConsoleLine(const core::LogRecord& record);
} // namespace engine::editor
