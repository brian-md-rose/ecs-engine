#pragma once
#include <filesystem>
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
	[[nodiscard]] SceneLoadResult LoadSceneFile(Registry& registry, const TypeRegistry& types,
	                                            const std::filesystem::path& file);
	bool SaveSceneFile(const Registry& registry, const TypeRegistry& types, const std::filesystem::path& file,
	                   Issues& issues);
} // namespace engine::ecs
