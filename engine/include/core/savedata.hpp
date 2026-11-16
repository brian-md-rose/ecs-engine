#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "core/text.hpp"

// A small store of named values a game keeps between runs (a high score, settings), saved as a
// text file of "key value" lines:
//
//     # A comment. Blank lines are ignored.
//     highScore 12340
//     playerName Ada Lovelace
//
// A key is one word; its value is the rest of the line, without the spaces around it. Pure apart
// from LoadSaveData and WriteSaveData: no SDL.
namespace engine::core
{
	class SaveData
	{
		std::map<std::string, std::string, std::less<>> values;

	public:
		// The value saved under key, if there is one.
		[[nodiscard]] std::optional<std::string_view> Get(std::string_view key) const;

		// The value saved under key as a whole number, if there is one and it is a number.
		[[nodiscard]] std::optional<int> GetInt(std::string_view key) const;

		// key must be one word (no spaces, tabs or line breaks, and not starting with '#');
		// value must fit on one line.
		void Set(std::string_view key, std::string_view value);
		void SetInt(std::string_view key, int value);

		[[nodiscard]] bool Empty() const noexcept { return values.empty(); }

		// One "key value" line per entry, sorted by key.
		[[nodiscard]] std::string Format() const;
	};

	// Either the data, or the first error found and no data.
	struct SaveDataResult
	{
		std::optional<SaveData> data;
		ParseError error;
	};

	// Reads save data written by Format. Every non-blank, non-comment line needs a key and a
	// value; a key given twice keeps the later value.
	[[nodiscard]] SaveDataResult ParseSaveData(std::string_view text);

	// Reads and parses a save file. A file that doesn't exist yet is empty data, not an error.
	[[nodiscard]] SaveDataResult LoadSaveData(const std::filesystem::path& file);

	// Writes data to file, making its folder if needed. It writes a temporary file beside it
	// first and then replaces file with it, so a crash part-way leaves the old save intact.
	// False if anything failed; the reason is logged.
	bool WriteSaveData(const std::filesystem::path& file, const SaveData& data);
} // namespace engine::core
