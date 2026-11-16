#include "save.hpp"

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include "core/paths.hpp"
#include "core/savedata.hpp"
#include "core/text.hpp"
#include "gameflow.hpp"
#include "hud.hpp"
#include "scoring.hpp"

namespace game
{
	namespace
	{
		// The save data in file, or empty data (reported on stderr) if it can't be read.
		[[nodiscard]] engine::core::SaveData ReadSave(const std::filesystem::path& file)
		{
			engine::core::SaveDataResult result = engine::core::LoadSaveData(file);
			if (result.data) return std::move(*result.data);
			const std::string message = engine::core::FormatError(engine::core::PathText(file), result.error);
			std::fprintf(stderr, "%s\n", message.c_str());
			return {};
		}
	} // namespace

	std::filesystem::path DefaultSaveFile()
	{
		const std::filesystem::path folder = engine::core::SaveDirectory(kSaveOrganisation, kSaveApplication);
		if (folder.empty()) return {};
		return folder / kSaveFileName;
	}

	int LoadHighScore(const std::filesystem::path& file)
	{
		if (file.empty()) return 0;
		return std::max(ReadSave(file).GetInt(kHighScoreKey).value_or(0), 0);
	}

	void SaveHighScore(engine::ecs::World& world)
	{
		SaveFile* save = world.resources.TryGet<SaveFile>();
		if (save == nullptr || save->path.empty()) return;

		const int best = std::max(world.resources.Get<HighScore>().points, world.resources.Get<Score>().points);
		if (best <= save->savedHighScore) return;

		engine::core::SaveData data = ReadSave(save->path);
		data.SetInt(kHighScoreKey, best);
		if (engine::core::WriteSaveData(save->path, data)) save->savedHighScore = best;
	}

	void SaveHighScoreOnStageChange(engine::ecs::World& world, const StageChanged& event)
	{
		if (event.to == Stage::GameOver || event.to == Stage::Title) SaveHighScore(world);
	}
} // namespace game
