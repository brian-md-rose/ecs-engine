#include "difficulty.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "core/assert.hpp"

namespace game
{
	namespace
	{
		using engine::core::ParseError;
		using engine::core::Word;

		// After "level <n>": four speeds and times, then the five timed stretches of the schedule.
		constexpr std::size_t kNumbers = 4 + kSchedulePhases - 1;
		constexpr float kMaxSpeed = 30.0f;    // tiles per second; much faster can't be steered
		constexpr float kMaxSeconds = 600.0f; // a typo guard, not a game rule

		constexpr std::array<std::string_view, kNumbers> kColumnNames = {
		    {"player speed", "enemy speed", "frightened speed", "fright seconds", "first patrol", "first chase",
		     "second patrol", "second chase", "third patrol"}};

		[[nodiscard]] DifficultyResult Fail(std::size_t line, std::size_t column, std::string message)
		{
			return {.table = std::nullopt, .error = {.line = line, .column = column, .message = std::move(message)}};
		}

		[[nodiscard]] bool IsBlankOrComment(std::string_view line)
		{
			const std::size_t first = line.find_first_not_of(" \t");
			return first == std::string_view::npos || line[first] == '#';
		}

		[[nodiscard]] ModeSchedule MakeSchedule(const std::array<float, kSchedulePhases - 1>& seconds)
		{
			ModeSchedule schedule{};
			for (std::size_t i = 0; i < kSchedulePhases; ++i)
			{
				schedule[i].mode = i % 2 == 0 ? ScheduledMode::Patrol : ScheduledMode::Chase;
				schedule[i].seconds = i < seconds.size() ? seconds[i] : 0.0f;
			}
			return schedule;
		}
	} // namespace

	DifficultyTable::DifficultyTable(std::vector<LevelTuning> perLevel) : levels(std::move(perLevel))
	{
		ENGINE_VERIFY(!levels.empty(), "a difficulty table needs at least one level");
	}

	const LevelTuning& DifficultyTable::ForLevel(int level) const noexcept
	{
		const int last = static_cast<int>(levels.size()) - 1;
		return levels[static_cast<std::size_t>(std::clamp(level - 1, 0, last))];
	}

	DifficultyTable FallbackDifficulty()
	{
		constexpr float kPlayer = 7.5f;
		constexpr float kEnemy = 6.75f;
		constexpr float kFrightened = 4.25f;
		const ModeSchedule early = MakeSchedule({{6.0f, 18.0f, 6.0f, 24.0f, 4.0f}});
		const ModeSchedule middle = MakeSchedule({{5.0f, 22.0f, 4.0f, 30.0f, 3.0f}});
		const ModeSchedule late = MakeSchedule({{4.0f, 26.0f, 3.0f, 40.0f, 2.0f}});
		return DifficultyTable({{kPlayer, kEnemy, kFrightened, 7.0f, early},
		                        {kPlayer, kEnemy, kFrightened, 6.0f, middle},
		                        {kPlayer, kEnemy, kFrightened, 5.0f, middle},
		                        {kPlayer, kEnemy, kFrightened, 4.0f, late},
		                        {kPlayer, kEnemy, kFrightened, 3.0f, late}});
	}

	DifficultyResult ParseDifficulty(std::string_view text)
	{
		const std::vector<std::string_view> lines = engine::core::SplitLines(text);
		std::vector<LevelTuning> levels;
		for (std::size_t index = 0; index < lines.size(); ++index)
		{
			const std::size_t line = index + 1;
			if (IsBlankOrComment(lines[index])) continue;

			const std::vector<Word> words = engine::core::SplitWords(lines[index]);
			if (words[0].text != "level")
			{
				return Fail(line, words[0].column, "expected 'level', not '" + std::string(words[0].text) + "'");
			}
			if (words.size() != kNumbers + 2)
			{
				return Fail(line, 0,
				            "expected 'level', the level's number and " + std::to_string(kNumbers) +
				                " numbers: player, enemy and frightened speed, fright seconds, then "
				                "patrol, chase, patrol, chase, patrol seconds");
			}

			const int expected = static_cast<int>(levels.size()) + 1;
			const std::optional<int> number = engine::core::ParseInt(words[1].text);
			if (!number || *number != expected)
			{
				return Fail(line, words[1].column,
				            "levels are numbered 1, 2, 3, ... in order: expected " + std::to_string(expected));
			}

			std::array<float, kNumbers> values{};
			for (std::size_t i = 0; i < kNumbers; ++i)
			{
				const Word& word = words[i + 2];
				const std::optional<float> value = engine::core::ParseFloat(word.text);
				const float limit = i < 3 ? kMaxSpeed : kMaxSeconds;
				if (!value || !(*value > 0.0f) || *value > limit)
				{
					return Fail(line, word.column,
					            "the " + std::string(kColumnNames[i]) + " must be a number above 0 and at most " +
					                std::to_string(static_cast<int>(limit)) + ", not '" + std::string(word.text) + "'");
				}
				values[i] = *value;
			}

			levels.push_back({.playerSpeed = values[0],
			                  .enemySpeed = values[1],
			                  .frightenedSpeed = values[2],
			                  .frightSeconds = values[3],
			                  .schedule = MakeSchedule({{values[4], values[5], values[6], values[7], values[8]}})});
		}

		if (levels.empty()) return Fail(0, 0, "no levels: add a 'level 1 ...' line");
		return {.table = DifficultyTable(std::move(levels)), .error = {}};
	}

	DifficultyResult LoadDifficulty(const std::filesystem::path& file)
	{
		const std::optional<std::string> text = engine::core::ReadTextFile(file);
		if (!text) return Fail(0, 0, "can't read the file");
		return ParseDifficulty(*text);
	}
} // namespace game
