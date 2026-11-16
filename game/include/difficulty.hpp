#pragma once
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "core/text.hpp"
#include "enemytuning.hpp"

// How hard each level is: speeds, the length of a fright and the patrol/chase schedule, read
// from a data file (assets/levels.txt, format in docs/difficulty.md) so the curve can be tuned
// without rebuilding. Pure apart from LoadDifficulty: no SDL.
namespace game
{
	// One level's numbers. Speeds are in tiles per second, times in seconds.
	struct LevelTuning
	{
		float playerSpeed = 0.0f;
		float enemySpeed = 0.0f;      // patrolling or chasing
		float frightenedSpeed = 0.0f; // frightened, in the maze or leaving the house
		float frightSeconds = 0.0f;   // how long a power pellet frightens the enemies
		ModeSchedule schedule{};      // patrol, chase, patrol, chase, patrol, then chase for good
	};

	// Resource: the tuning of every level. Levels past the last listed play like the last.
	class DifficultyTable
	{
		std::vector<LevelTuning> levels;

	public:
		// levels[0] is level 1. Must not be empty.
		explicit DifficultyTable(std::vector<LevelTuning> perLevel);

		// Counting levels from 1; below 1 counts as 1, beyond the table as the last level.
		[[nodiscard]] const LevelTuning& ForLevel(int level) const noexcept;

		[[nodiscard]] std::size_t LevelCount() const noexcept { return levels.size(); }
	};

	// The Phase 6 tuning for levels 1-5, for games built without a levels file (the tests).
	// The real game always loads assets/levels.txt.
	[[nodiscard]] DifficultyTable FallbackDifficulty();

	// Either a table, or the first error found and no table.
	struct DifficultyResult
	{
		std::optional<DifficultyTable> table;
		engine::core::ParseError error;
	};

	// Parses a levels file: one "level" line per level, numbered 1, 2, 3, ... in order. Errors
	// give the line and column of the offending number.
	[[nodiscard]] DifficultyResult ParseDifficulty(std::string_view text);

	// Reads a file and parses it. A file that can't be read is an error about the whole file.
	[[nodiscard]] DifficultyResult LoadDifficulty(const std::filesystem::path& file);
} // namespace game
