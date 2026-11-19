#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assets.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/states.hpp"
#include "ecs/typeregistry.hpp"
#include "ecs/world.hpp"

// The screens' layouts are scene files (assets/scenes/<name>.json, see docs/scenes.md and docs/screens.md): the title,
// help, options, name entry, level transition and pause screens, and the UI demo. A screen's enter system loads its
// file with SpawnScene, scopes what it made to the state, and fills in what only the game knows (the high score, the
// help rows). Because a screen is data, it can be opened, edited and played in the editor.
namespace game
{
	// Resource: the parsed scene files, by name (the file name without .json). Filled from GameAssets::scenes.
	struct GameScenes
	{
		SceneFiles files;

		// The scene called name, or null if there is none.
		[[nodiscard]] const engine::core::JsonValue* Find(std::string_view name) const noexcept;
	};

	// Reads every .json file in folder (assets/scenes) into a SceneFiles, keyed by file name without the extension. A
	// file that can't be read or parsed is logged and makes the result empty (nullopt), as for any other asset.
	[[nodiscard]] std::optional<SceneFiles> LoadSceneFolder(const std::filesystem::path& folder);

	// Describes the components of the menu screens (MenuButton, ScreenText, HelpList) and the UI demo's to a type
	// registry, so their scene files can be loaded and edited. Call it after RegisterEngineComponents.
	void RegisterGameComponents(engine::ecs::TypeRegistry& types);

	// Loads the scene called name into the world and returns the entities it made, in file order, with the pictures
	// named by ClipName resolved. A missing or broken scene leaves the screen empty and says why in the log (the
	// returned list is empty).
	std::vector<engine::ecs::Entity> SpawnScene(engine::ecs::World& world, std::string_view name);

	// SpawnScene, with every entity scoped to the state T = scope, so leaving that state takes them away.
	template <typename T>
	std::vector<engine::ecs::Entity> SpawnScene(engine::ecs::World& world, std::string_view name, T scope)
	{
		std::vector<engine::ecs::Entity> entities = SpawnScene(world, name);
		for (const engine::ecs::Entity entity : entities)
		{
			world.registry.Add<engine::ecs::DespawnOnExit<T>>(entity, engine::ecs::DespawnOnExit<T>{scope});
		}
		return entities;
	}
} // namespace game
