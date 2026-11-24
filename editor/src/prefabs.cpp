#include "editor/prefabs.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <system_error>

#include "core/text.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/prefabinstance.hpp"
#include "ecs/scene.hpp"
#include "editor/entityops.hpp"
#include "editor/viewport.hpp"

namespace engine::editor
{
	namespace
	{
		std::string FirstProblem(const ecs::Issues& issues, const std::string& fallback)
		{
			if (issues.errors.empty()) return fallback;
			const ecs::Issue& issue = issues.errors.front();
			return issue.path.empty() ? issue.message : issue.path + ": " + issue.message;
		}

		std::filesystem::path FileOf(const std::filesystem::path& folder, const std::string& name)
		{
			return folder / (name + ".json");
		}
	} // namespace

	std::filesystem::path PrefabFolder(const std::filesystem::path& assetsFolder)
	{
		return assetsFolder / "prefabs";
	}

	bool IsValidPrefabName(const std::string& name)
	{
		return !name.empty() && name.find_first_of("/\\:.") == std::string::npos;
	}

	std::vector<std::string> ListPrefabs(const std::filesystem::path& folder)
	{
		std::vector<std::string> names;
		std::error_code error;
		for (const auto& entry : std::filesystem::directory_iterator(folder, error))
			if (entry.is_regular_file() && entry.path().extension() == ".json")
				names.push_back(entry.path().stem().string());
		std::sort(names.begin(), names.end());
		return names;
	}

	bool SavePrefab(ecs::Registry& registry, const ecs::TypeRegistry& types, ecs::Entity root,
	                const std::filesystem::path& folder, const std::string& name, std::string& error)
	{
		if (!IsValidPrefabName(name))
		{
			error = "a prefab name can't be empty or have dots, slashes or colons";
			return false;
		}
		if (!registry.IsAlive(root))
		{
			error = "the entity no longer exists";
			return false;
		}
		// The file records where it came from, so a copy made from it knows too. Put back if saving fails.
		const bool hadInstance = registry.HasComponent<ecs::PrefabInstance>(root);
		const std::string previous = hadInstance ? registry.TryGetComponent<ecs::PrefabInstance>(root)->name : "";
		if (hadInstance)
			registry.TryGetComponent<ecs::PrefabInstance>(root)->name = name;
		else
			registry.Add<ecs::PrefabInstance>(root, ecs::PrefabInstance{name});
		const auto undo = [&]
		{
			if (hadInstance)
				registry.TryGetComponent<ecs::PrefabInstance>(root)->name = previous;
			else
				registry.Remove<ecs::PrefabInstance>(root);
		};

		ecs::Issues issues;
		const std::array<ecs::Entity, 1> roots{root};
		const std::string text = CopyTrees(registry, types, roots, issues);
		if (text.empty())
		{
			undo();
			error = FirstProblem(issues, "nothing to save");
			return false;
		}
		std::error_code fsError;
		std::filesystem::create_directories(folder, fsError);
		std::ofstream out(FileOf(folder, name), std::ios::binary | std::ios::trunc);
		out << text;
		out.flush();
		if (!out)
		{
			undo();
			error = "can't write " + FileOf(folder, name).string();
			return false;
		}
		return true;
	}

	ecs::Entity InstantiatePrefab(ecs::Registry& registry, const ecs::TypeRegistry& types,
	                              const std::filesystem::path& folder, const std::string& name, std::string& error)
	{
		if (!IsValidPrefabName(name))
		{
			error = "not a prefab name: " + name;
			return ecs::Entity::Null;
		}
		const std::optional<std::string> text = core::ReadTextFile(FileOf(folder, name));
		if (!text)
		{
			error = "no prefab file " + FileOf(folder, name).string();
			return ecs::Entity::Null;
		}
		ecs::Issues issues;
		const std::vector<ecs::Entity> roots = PasteTrees(registry, types, *text, issues);
		if (roots.empty())
		{
			error = FirstProblem(issues, "the prefab holds no entities");
			return ecs::Entity::Null;
		}
		if (roots.size() != 1)
		{
			// Not a tree: put it back as it was.
			for (const ecs::Entity extra : roots)
				DestroyTree(registry, extra);
			error = "a prefab must have exactly one root entity, " + name + " has " + std::to_string(roots.size());
			return ecs::Entity::Null;
		}
		// Whatever the file says, the copy knows its prefab by the file's name.
		if (auto* instance = registry.TryGetComponent<ecs::PrefabInstance>(roots.front()))
			instance->name = name;
		else
			registry.Add<ecs::PrefabInstance>(roots.front(), ecs::PrefabInstance{name});
		return roots.front();
	}

	std::string PrefabNameOf(const ecs::Registry& registry, ecs::Entity root)
	{
		if (!registry.IsAlive(root)) return {};
		const auto* instance = registry.TryGetComponent<ecs::PrefabInstance>(root);
		return instance == nullptr ? std::string{} : instance->name;
	}

	bool ApplyToPrefab(ecs::Registry& registry, const ecs::TypeRegistry& types, ecs::Entity root,
	                   const std::filesystem::path& folder, std::string& error)
	{
		const std::string name = PrefabNameOf(registry, root);
		if (name.empty())
		{
			error = "this entity is not an instance of a prefab";
			return false;
		}
		return SavePrefab(registry, types, root, folder, name, error);
	}

	ecs::Entity RevertToPrefab(ecs::Registry& registry, const ecs::TypeRegistry& types, ecs::Entity root,
	                           const std::filesystem::path& folder, std::string& error)
	{
		const std::string name = PrefabNameOf(registry, root);
		if (name.empty())
		{
			error = "this entity is not an instance of a prefab";
			return ecs::Entity::Null;
		}
		// The new copy first, so a prefab that can't be loaded leaves the scene as it was.
		const ecs::Entity fresh = InstantiatePrefab(registry, types, folder, name, error);
		if (fresh.IsNull()) return ecs::Entity::Null;
		ecs::Entity parent = ecs::Entity::Null;
		if (const auto* link = registry.TryGetComponent<ecs::Parent>(root)) parent = link->entity;
		const std::optional<graphics::Position> at = MovablePosition(registry, root);
		DestroyTree(registry, root);
		if (!parent.IsNull() && registry.IsAlive(parent)) (void)SetParent(registry, fresh, parent);
		if (at) (void)SetMovablePosition(registry, fresh, *at);
		return fresh;
	}
} // namespace engine::editor
