#include "save.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/savedata.hpp"
#include "core/text.hpp"
#include "hud.hpp"
#include "nameentry.hpp"
#include "scoring.hpp"

namespace game
{
	namespace
	{
		// The save data in file, or empty data (logged) if it can't be read.
		[[nodiscard]] engine::core::SaveData ReadSave(const std::filesystem::path& file)
		{
			engine::core::SaveDataResult result = engine::core::LoadSaveData(file);
			if (result.data) return std::move(*result.data);
			const std::string message = engine::core::FormatError(engine::core::PathText(file), result.error);
			engine::core::LogError(message);
			return {};
		}
	} // namespace

	std::filesystem::path DefaultSaveFile()
	{
		const std::filesystem::path folder = engine::core::SaveDirectory(kSaveOrganisation, kSaveApplication);
		if (folder.empty()) return {};
		return folder / kSaveFileName;
	}

	SavedGame LoadSavedGame(const std::filesystem::path& file)
	{
		SavedGame saved;
		if (file.empty()) return saved;
		const engine::core::SaveData data = ReadSave(file);
		saved.highScore = std::max(data.GetInt(kHighScoreKey).value_or(0), 0);
		saved.highScoreName = CleanName(data.Get(kHighScoreNameKey).value_or(""));
		saved.settings = SettingsFrom(data);
		return saved;
	}

	void ApplySavedGame(engine::ecs::World& world, const SavedGame& saved)
	{
		SaveFile& save = world.resources.Get<SaveFile>();
		save.savedHighScore = saved.highScore;
		save.savedName = saved.highScoreName;

		HighScore& high = world.resources.Get<HighScore>();
		high.points = saved.highScore;
		high.record = saved.highScore;
		high.name = saved.highScoreName;

		world.resources.Get<Settings>() = saved.settings;
		if (!saved.highScoreName.empty()) world.resources.Get<NameEntry>() = EntryFor(saved.highScoreName);
	}

	void RecordScore(engine::ecs::World& world, std::string_view name)
	{
		HighScore& high = world.resources.Get<HighScore>();
		const int score = world.resources.Get<Score>().points;
		if (score <= high.record) return;
		high.record = score;
		high.name = std::string(name);
		high.points = std::max(high.points, score);
	}

	void SaveHighScore(engine::ecs::World& world)
	{
		SaveFile* save = world.resources.TryGet<SaveFile>();
		if (save == nullptr || save->path.empty()) return;

		const HighScore& high = world.resources.Get<HighScore>();
		if (high.record <= save->savedHighScore) return;

		engine::core::SaveData data = ReadSave(save->path);
		data.SetInt(kHighScoreKey, high.record);
		if (high.name.empty())
		{
			data.Set(kHighScoreNameKey, std::string(kDefaultName));
		}
		else
		{
			data.Set(kHighScoreNameKey, high.name);
		}
		if (engine::core::WriteSaveData(save->path, data))
		{
			save->savedHighScore = high.record;
			save->savedName = high.name;
		}
	}

	void SaveSettings(engine::ecs::World& world)
	{
		const SaveFile* save = world.resources.TryGet<SaveFile>();
		if (save == nullptr || save->path.empty()) return;

		engine::core::SaveData data = ReadSave(save->path);
		StoreSettings(data, world.resources.Get<Settings>());
		(void)engine::core::WriteSaveData(save->path, data);
	}
} // namespace game
