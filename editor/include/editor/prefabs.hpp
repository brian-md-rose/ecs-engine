#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/typeregistry.hpp"

// Basic prefabs: an entity tree saved as a file in a prefabs folder (the assets folder's `prefabs`), and put into a
// scene again as copies. A copy remembers where it came from by a PrefabInstance on its root, so it can be applied
// back to the prefab or reverted to it. Plain functions on a Registry, so they can be tested without a window.
// A file is the same text as the clipboard's copy (see CopyTrees), so a prefab can be read and edited by hand.
namespace engine::editor
{
	// Where the prefabs of an assets folder are.
	[[nodiscard]] std::filesystem::path PrefabFolder(const std::filesystem::path& assetsFolder);

	// A name that is safe to use as a file name: not empty, no folders or dots, no ".json" (that is added).
	[[nodiscard]] bool IsValidPrefabName(const std::string& name);

	// The prefabs in folder, by name (without ".json"), sorted.
	[[nodiscard]] std::vector<std::string> ListPrefabs(const std::filesystem::path& folder);

	// Saves root and everything under it as `<name>.json` in folder (made if needed, an existing file replaced), and
	// gives root a PrefabInstance{name}. False with a message in error if the name is not valid, the tree refers to
	// entities outside it, or the file can't be written; root is left alone then.
	bool SavePrefab(ecs::Registry& registry, const ecs::TypeRegistry& types, ecs::Entity root,
	                const std::filesystem::path& folder, const std::string& name, std::string& error);

	// Creates a copy of the prefab at the top level of the scene and returns its root, which has a PrefabInstance.
	// Null with a message in error if the file is missing or does not load; nothing is kept then.
	[[nodiscard]] ecs::Entity InstantiatePrefab(ecs::Registry& registry, const ecs::TypeRegistry& types,
	                                            const std::filesystem::path& folder, const std::string& name,
	                                            std::string& error);

	// The prefab that root is an instance of: its PrefabInstance name, or empty if it has none.
	[[nodiscard]] std::string PrefabNameOf(const ecs::Registry& registry, ecs::Entity root);

	// Saves the tree under root over the prefab it is an instance of. False (with error) if root is not an instance.
	bool ApplyToPrefab(ecs::Registry& registry, const ecs::TypeRegistry& types, ecs::Entity root,
	                   const std::filesystem::path& folder, std::string& error);

	// Replaces the tree under root with a fresh copy of its prefab, keeping where the root is (its parent and its
	// position; its other fields come from the prefab). Returns the new root; Null
	// with a message in error if root is not an instance or the prefab can't be loaded, and then nothing changes.
	[[nodiscard]] ecs::Entity RevertToPrefab(ecs::Registry& registry, const ecs::TypeRegistry& types, ecs::Entity root,
	                                         const std::filesystem::path& folder, std::string& error);
} // namespace engine::editor
