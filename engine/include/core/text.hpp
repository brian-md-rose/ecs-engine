#pragma once
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Small helpers for the engine's and games' line-based text formats (pixel sheets, data
// tables). Pure apart from ReadTextFile: no SDL.
namespace engine::core
{
	// Why a text file was rejected. line and column count from 1 and point at the offending
	// character; both are 0 for errors about the file as a whole, and column alone is 0 for
	// errors about a whole line.
	struct ParseError
	{
		std::size_t line = 0;
		std::size_t column = 0;
		std::string message;
	};

	// "source:line:column: message", "source:line: message" or "source: message", the way
	// compilers write them, so editors can jump to the spot.
	[[nodiscard]] std::string FormatError(std::string_view source, const ParseError& error);

	// One entry per line, without the line ending (LF or CRLF). Line i of the file is entry
	// i - 1. Blank lines at the end are dropped, including the empty "line" after a final newline.
	[[nodiscard]] std::vector<std::string_view> SplitLines(std::string_view text);

	// A word of a line and where it starts, counting columns from 1.
	struct Word
	{
		std::string_view text;
		std::size_t column = 0;
	};

	// The words of line, split on spaces and tabs.
	[[nodiscard]] std::vector<Word> SplitWords(std::string_view line);

	// The whole of text as a number, or nothing if it isn't one (or has anything after it).
	[[nodiscard]] std::optional<int> ParseInt(std::string_view text);
	[[nodiscard]] std::optional<float> ParseFloat(std::string_view text);

	// The bytes of file, or nothing if it can't be read.
	[[nodiscard]] std::optional<std::string> ReadTextFile(const std::filesystem::path& file);

	// The path as UTF-8 text, for messages.
	[[nodiscard]] std::string PathText(const std::filesystem::path& file);
} // namespace engine::core
