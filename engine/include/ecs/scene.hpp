#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "core/json.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/typeregistry.hpp"

// Scenes: a set of entities and their components saved as JSON and loaded back. Built on the
// TypeRegistry, so only registered, non-Runtime components are saved. See docs/scenes.md.
//
//     {
//       "format": 1,
//       "engine": "0.6.0",
//       "entities": [
//         { "id": 1, "name": "Player", "components": { "Transform": { "position": { "x": 16, "y": 24 } } } },
//         { "id": 2, "name": "Label", "parent": 1, "components": { ... } }
//       ]
//     }
//
// An entity's id is its number in the file, not its Entity: an Entity's index and generation change
// from run to run. `name` is the Name component and `parent` the Parent component; neither
// appears under "components". Entity-valued fields are written as ids and remapped on load.
namespace engine::ecs
{
	// The scene format this engine writes and reads.
	inline constexpr int kSceneFormat = 1;

	// What a scene says about how it is drawn, beside its entities. Both parts are optional: a scene that leaves
	// one out leaves it to the game. Stored under "settings" in the file ("camera": {"x", "y", "zoom"} and
	// "background": [r, g, b]); a reader that doesn't know it ignores it. It is a plain value, kept in a world as a
	// resource so the game and the editor see the same one.
	struct SceneCamera
	{
		float x = 0.0F;
		float y = 0.0F;
		int zoom = 1;
		bool operator==(const SceneCamera&) const = default;
	};

	struct SceneSettings
	{
		std::optional<SceneCamera> camera;
		std::optional<std::array<std::uint8_t, 3>> background;

		[[nodiscard]] bool Empty() const noexcept { return !camera && !background; }
		bool operator==(const SceneSettings&) const = default;
	};

	// The settings of a scene's JSON: empty if it has none. A part that is the wrong shape or out of range is left
	// out and reported as a warning in issues (a bad look should not stop a scene from loading).
	[[nodiscard]] SceneSettings ReadSceneSettings(const core::JsonValue& scene, Issues& issues);

	// Puts settings into a scene's JSON, replacing what was there; empty settings remove the "settings" entry.
	void WriteSceneSettings(core::JsonValue& scene, const SceneSettings& settings);

	struct SceneLoadResult
	{
		std::vector<Entity> entities; // the entities created, in file order; empty if the load failed
		Issues issues;

		[[nodiscard]] bool Ok() const noexcept { return issues.Ok(); }
	};

	// Every live entity, in order of id. Components of types that are not registered, or are
	// Runtime, are left out. Problems (a reference to an entity that is not saved) go in issues;
	// the JSON is still returned, with the offending field missing.
	[[nodiscard]] core::JsonValue SaveScene(const Registry& registry, const TypeRegistry& types, Issues& issues);

	// Only the listed entities (for a selection or a prefab). A reference to one that is not listed
	// is an error.
	[[nodiscard]] core::JsonValue SaveScene(const Registry& registry, const TypeRegistry& types, Issues& issues,
	                                        std::span<const Entity> entities);

	// Creates the scene's entities in registry, adding components through Registry::Add so OnAdd
	// hooks run. Every problem found is reported with a path such as
	// "entities[3].components.Follow.target". A component name that is not registered is a warning,
	// so a scene survives a removed component. If there is any error, the entities created are
	// destroyed again and none is kept.
	[[nodiscard]] SceneLoadResult LoadScene(Registry& registry, const TypeRegistry& types,
	                                        const core::JsonValue& scene);

	// File versions of the above, through core::LoadJson and core::WriteJson. A file that can't be
	// read or parsed is an error at path "". SaveSceneFile is false if there were issues or the
	// write failed.
	// LoadSceneFile also reads the file's settings into *settings, if given (empty if it has none, or the load fails).
	// SaveSceneFile writes *settings, if given.
	[[nodiscard]] SceneLoadResult LoadSceneFile(Registry& registry, const TypeRegistry& types,
	                                            const std::filesystem::path& file, SceneSettings* settings = nullptr);
	bool SaveSceneFile(const Registry& registry, const TypeRegistry& types, const std::filesystem::path& file,
	                   Issues& issues, const SceneSettings* settings = nullptr);
} // namespace engine::ecs
