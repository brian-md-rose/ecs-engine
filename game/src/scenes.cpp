#include "scenes.hpp"

#include <algorithm>

#include "core/json.hpp"
#include "core/log.hpp"
#include "core/text.hpp"
#include "ecs/scene.hpp"
#include "hud.hpp"
#include "mazescene.hpp"
#include "menuui.hpp"
#include "screens.hpp"
#include "uidemo.hpp"

namespace game
{
	const engine::core::JsonValue* GameScenes::Find(std::string_view name) const noexcept
	{
		const auto found = files.find(name);
		return found == files.end() ? nullptr : &found->second;
	}

	std::optional<SceneFiles> LoadSceneFolder(const std::filesystem::path& folder)
	{
		std::error_code error;
		std::vector<std::filesystem::path> paths;
		for (const auto& entry : std::filesystem::directory_iterator(folder, error))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".json") paths.push_back(entry.path());
		}
		if (error)
		{
			engine::core::LogError(engine::core::PathText(folder) +
			                       ": can't read the scenes folder: " + error.message());
			return std::nullopt;
		}
		std::sort(paths.begin(), paths.end());

		SceneFiles scenes;
		for (const std::filesystem::path& path : paths)
		{
			engine::core::JsonResult scene = engine::core::LoadJson(path);
			if (!scene.value)
			{
				engine::core::LogError(engine::core::FormatError(engine::core::PathText(path), scene.error));
				return std::nullopt;
			}
			scenes.emplace(path.stem().string(), std::move(*scene.value));
		}
		return scenes;
	}

	void RegisterGameComponents(engine::ecs::TypeRegistry& types)
	{
		RegisterUiDemoComponents(types);
		RegisterMazeComponents(types);
		types.Register<MenuButton>("MenuButton")
		    .EnumField("menu", &MenuButton::menu,
		               {{"Title", MenuId::Title},
		                {"Options", MenuId::Options},
		                {"Pause", MenuId::Pause},
		                {"Help", MenuId::Help}})
		    .Field("item", &MenuButton::item);
		types.RegisterTag<HelpList>("HelpList");
		types.RegisterTag<HudLabel>("HudLabel");
		types.Register<HudText>("HudText")
		    .EnumField("field", &HudText::field,
		               {{"Score", HudField::Score},
		                {"HighScore", HudField::HighScore},
		                {"Level", HudField::Level},
		                {"Lives", HudField::Lives},
		                {"Message", HudField::Message}})
		    .Field("anchorX", &HudText::anchorX)
		    .EnumField("align", &HudText::align,
		               {{"Left", engine::graphics::Align::Left},
		                {"Centre", engine::graphics::Align::Centre},
		                {"Right", engine::graphics::Align::Right}});
		types.Register<ScreenText>("ScreenText")
		    .EnumField("kind", &ScreenText::kind,
		               {{"TitleItem", TextKind::TitleItem},
		                {"PauseItem", TextKind::PauseItem},
		                {"HelpBack", TextKind::HelpBack},
		                {"OptionItem", TextKind::OptionItem},
		                {"NameSlot", TextKind::NameSlot},
		                {"HighScoreLabel", TextKind::HighScoreLabel},
		                {"HighScoreValue", TextKind::HighScoreValue},
		                {"ScoreLine", TextKind::ScoreLine},
		                {"LevelLine", TextKind::LevelLine}})
		    .Field("label", &ScreenText::label)
		    .Field("item", &ScreenText::item);
	}

	std::vector<engine::ecs::Entity> SpawnScene(engine::ecs::World& world, std::string_view name)
	{
		const std::string source = "scene " + std::string(name);
		const engine::core::JsonValue* json = world.resources.Get<GameScenes>().Find(name);
		if (json == nullptr)
		{
			engine::core::LogError(source + ": no such scene file (assets/scenes/" + std::string(name) + ".json)");
			return {};
		}

		const engine::ecs::SceneLoadResult loaded =
		    engine::ecs::LoadScene(world.registry, world.resources.Get<engine::ecs::TypeRegistry>(), *json);
		for (const engine::ecs::Issue& issue : loaded.issues.errors)
		{
			engine::core::LogError(source + ": " + issue.path + ": " + issue.message);
		}
		for (const engine::ecs::Issue& issue : loaded.issues.warnings)
		{
			engine::core::LogWarning(source + ": " + issue.path + ": " + issue.message);
		}
		if (!loaded.Ok()) return {};

		ResolveClipNames(world.registry, world.resources.Get<SpriteClips>());
		return loaded.entities;
	}
} // namespace game
