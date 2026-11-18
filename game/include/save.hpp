#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "ecs/world.hpp"
#include "options.hpp"

// What the game keeps between runs, in save.txt in the user's save folder
// (engine::core::SaveDirectory; on Windows %APPDATA%\ecs-engine\Tangle Run): the high score and
// the name of whoever set it, and the options. It is loaded when the game starts. The high
// score is saved when a game ends, and the options when the options screen is left; both are
// also saved when the game closes. Writing keeps any other line in the file.
namespace game
{
	inline constexpr const char* kSaveOrganisation = "ecs-engine";
	inline constexpr const char* kSaveApplication = "Tangle Run";
	inline constexpr std::string_view kSaveFileName = "save.txt";
	inline constexpr std::string_view kHighScoreKey = "highScore";
	inline constexpr std::string_view kHighScoreNameKey = "highScoreName";

	// Resource: where the game saves, and the high score last saved there. An empty path saves
	// nothing, which is how the tests run.
	struct SaveFile
	{
		std::filesystem::path path{};
		int savedHighScore = 0;
		std::string savedName{};
	};

	// Everything a save file holds.
	struct SavedGame
	{
		int highScore = 0;
		std::string highScoreName{};
		Settings settings{};
	};

	// save.txt in the user's save folder; empty if the platform has none.
	[[nodiscard]] std::filesystem::path DefaultSaveFile();

	// What is saved in file: the defaults for anything missing or no file at all. A file that
	// can't be read is logged and counts as empty, so a broken save never stops the
	// game.
	[[nodiscard]] SavedGame LoadSavedGame(const std::filesystem::path& file);

	// Puts saved into the HighScore, NameEntry (as the name to start from), Settings and SaveFile
	// resources.
	void ApplySavedGame(engine::ecs::World& world, const SavedGame& saved);

	// Makes the score the record, held by name, if it beats the record.
	void RecordScore(engine::ecs::World& world, std::string_view name);

	// Saves the record and its holder to the SaveFile if the record beats the one saved there.
	// Does nothing without a SaveFile path.
	void SaveHighScore(engine::ecs::World& world);

	// Saves the Settings to the SaveFile. Does nothing without a path.
	void SaveSettings(engine::ecs::World& world);
} // namespace game
