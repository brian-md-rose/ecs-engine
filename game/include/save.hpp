#pragma once

#include <filesystem>
#include <string_view>

#include "ecs/world.hpp"
#include "events.hpp"

// The high score, kept between runs in save.txt in the user's save folder
// (engine::core::SaveDirectory; on Windows %APPDATA%\ecs-engine\Tangle Run). It is loaded when
// the game starts and saved whenever a game ends or is abandoned, and when the game closes.
namespace game
{
	inline constexpr const char* kSaveOrganisation = "ecs-engine";
	inline constexpr const char* kSaveApplication = "Tangle Run";
	inline constexpr std::string_view kSaveFileName = "save.txt";
	inline constexpr std::string_view kHighScoreKey = "highScore";

	// Resource: where the game saves, and the high score last saved there. An empty path saves
	// nothing, which is how the tests run.
	struct SaveFile
	{
		std::filesystem::path path{};
		int savedHighScore = 0;
	};

	// save.txt in the user's save folder; empty if the platform has none.
	[[nodiscard]] std::filesystem::path DefaultSaveFile();

	// The high score saved in file: 0 if there is none yet. A file that can't be read is reported
	// on stderr and counts as no high score, so a broken save never stops the game.
	[[nodiscard]] int LoadHighScore(const std::filesystem::path& file);

	// Saves the best of the HighScore and the Score to the SaveFile if it beats the one saved
	// there, keeping anything else in the file. Does nothing without a SaveFile path.
	void SaveHighScore(engine::ecs::World& world);

	// Event handler: saves the high score when a game ends (GameOver) or is abandoned (back to
	// the Title).
	void SaveHighScoreOnStageChange(engine::ecs::World& world, const StageChanged& event);
} // namespace game
