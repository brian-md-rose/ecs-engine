#include "editor/entityops.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/json.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/name.hpp"
#include "ecs/scene.hpp"

namespace engine::editor
{
	using ecs::Entity;
	using ecs::Registry;

	LiveIds::LiveIds(const Registry& world) : registry(world)
	{
		for (const Entity entity : world.AllEntities())
			alive[entity.id] = entity;
	}

	std::optional<std::int64_t> LiveIds::IdOf(Entity entity) const
	{
		if (!registry.IsAlive(entity)) return std::nullopt;
		return static_cast<std::int64_t>(entity.id);
	}

	std::optional<Entity> LiveIds::EntityOf(std::int64_t id) const
	{
		if (id < 0 || id > static_cast<std::int64_t>(UINT32_MAX)) return std::nullopt;
		const auto key = static_cast<std::uint32_t>(id);
		if (const auto replaced = overrides.find(key); replaced != overrides.end()) return replaced->second;
		if (const auto found = alive.find(key); found != alive.end()) return found->second;
		return std::nullopt;
	}

	std::string LabelOf(const Registry& registry, Entity entity)
	{
		if (const ecs::Name* name = registry.TryGetComponent<ecs::Name>(entity);
		    name != nullptr && !name->value.empty())
		{
			return name->value;
		}
		return "Entity " + std::to_string(entity.id);
	}

	Entity CreateEntity(Registry& registry, const std::string& name, Entity parent)
	{
		const Entity entity = registry.Create();
		registry.Add<ecs::Name>(entity, ecs::Name{name});
		if (!parent.IsNull() && registry.IsAlive(parent)) registry.Add<ecs::Parent>(entity, ecs::Parent{parent});
		return entity;
	}

	bool WouldMakeLoop(const Registry& registry, Entity child, Entity parent)
	{
		// Walk up from the new parent; meeting the child means the child would be its own ancestor. The step
		// count stops a loop that is already there from hanging this check.
		Entity current = parent;
		for (std::size_t steps = 0; !current.IsNull() && steps <= registry.EntityCount(); ++steps)
		{
			if (current == child) return true;
			const ecs::Parent* above = registry.TryGetComponent<ecs::Parent>(current);
			if (above == nullptr) return false;
			current = above->entity;
		}
		return !current.IsNull(); // ran out of steps: a loop above, so refuse
	}

	bool SetParent(Registry& registry, Entity child, Entity parent)
	{
		if (!registry.IsAlive(child)) return false;
		if (parent.IsNull())
		{
			registry.Remove<ecs::Parent>(child);
			return true;
		}
		if (!registry.IsAlive(parent) || WouldMakeLoop(registry, child, parent)) return false;
		if (ecs::Parent* existing = registry.TryGetComponent<ecs::Parent>(child))
		{
			existing->entity = parent;
		}
		else
		{
			registry.Add<ecs::Parent>(child, ecs::Parent{parent});
		}
		return true;
	}

	void DestroyTree(Registry& registry, Entity entity)
	{
		if (!registry.IsAlive(entity)) return;
		std::vector<Entity> doomed = ecs::DescendantsOf(registry, entity); // parents before children
		std::reverse(doomed.begin(), doomed.end());
		for (const Entity descendant : doomed)
		{
			if (registry.IsAlive(descendant)) registry.Destroy(descendant);
		}
		registry.Destroy(entity);
	}

	Entity DuplicateTree(Registry& registry, const ecs::TypeRegistry& types, Entity root, ecs::Issues& issues)
	{
		if (!registry.IsAlive(root))
		{
			issues.errors.push_back({"", "the entity no longer exists"});
			return Entity::Null;
		}
		std::vector<Entity> originals{root};
		const std::vector<Entity> below = ecs::DescendantsOf(registry, root);
		originals.insert(originals.end(), below.begin(), below.end());

		LiveIds ids(registry);
		std::vector<Entity> copies;
		copies.reserve(originals.size());
		for (const Entity original : originals)
		{
			const Entity copy = registry.Create();
			copies.push_back(copy);
			ids.Override(original.id, copy);
		}

		const std::size_t errorsBefore = issues.errors.size();
		bool failed = false;
		for (std::size_t i = 0; i < originals.size(); ++i)
		{
			for (std::size_t t = 0; t < types.Count(); ++t)
			{
				const ecs::TypeInfo& type = types.At(t);
				if (type.IsRuntime() || !type.Has(registry, originals[i])) continue;
				core::JsonValue saved;
				const std::string path = LabelOf(registry, originals[i]) + "." + type.Name();
				if (!type.Save(registry, originals[i], ids, saved, issues, path) ||
				    !type.Load(registry, copies[i], saved, ids, issues, path))
				{
					failed = true;
					if (issues.errors.size() == errorsBefore) issues.errors.push_back({path, "could not be copied"});
				}
			}
		}

		if (failed || issues.errors.size() != errorsBefore)
		{
			for (auto it = copies.rbegin(); it != copies.rend(); ++it)
				registry.Destroy(*it);
			return Entity::Null;
		}
		if (ecs::Name* name = registry.TryGetComponent<ecs::Name>(copies.front())) name->value += " (copy)";
		return copies.front();
	}

	std::string CopyTrees(Registry& registry, const ecs::TypeRegistry& types, std::span<const Entity> roots,
	                      ecs::Issues& issues)
	{
		std::vector<Entity> all;
		for (const Entity root : roots)
		{
			if (!registry.IsAlive(root)) continue;
			all.push_back(root);
			const std::vector<Entity> below = ecs::DescendantsOf(registry, root);
			all.insert(all.end(), below.begin(), below.end());
		}
		if (all.empty()) return {};
		ecs::Issues found;
		const core::JsonValue scene = ecs::SaveScene(registry, types, found, all);
		// A root's own parent is outside the copy on purpose; SaveScene leaves that field out.
		for (const ecs::Issue& issue : found.errors)
		{
			if (issue.message == "the parent is not part of the scene") continue;
			issues.errors.push_back(issue);
		}
		issues.warnings.insert(issues.warnings.end(), found.warnings.begin(), found.warnings.end());
		if (!issues.Ok()) return {};
		return core::FormatJson(scene);
	}

	std::vector<Entity> PasteTrees(Registry& registry, const ecs::TypeRegistry& types, std::string_view text,
	                               ecs::Issues& issues)
	{
		const core::JsonResult parsed = core::ParseJson(text);
		if (!parsed.value)
		{
			issues.errors.push_back({"", "the clipboard does not hold copied entities"});
			return {};
		}
		const ecs::SceneLoadResult loaded = ecs::LoadScene(registry, types, *parsed.value);
		issues.errors.insert(issues.errors.end(), loaded.issues.errors.begin(), loaded.issues.errors.end());
		issues.warnings.insert(issues.warnings.end(), loaded.issues.warnings.begin(), loaded.issues.warnings.end());
		std::vector<Entity> roots;
		if (!loaded.Ok()) return roots;
		for (const Entity entity : loaded.entities)
			if (!registry.HasComponent<ecs::Parent>(entity)) roots.push_back(entity);
		return roots;
	}

	Tree BuildTree(const Registry& registry)
	{
		Tree tree;
		tree.all = registry.AllEntities();
		for (const Entity entity : tree.all)
		{
			const ecs::Parent* parent = registry.TryGetComponent<ecs::Parent>(entity);
			if (parent != nullptr && registry.IsAlive(parent->entity))
			{
				tree.children[parent->entity].push_back(entity);
			}
			else
			{
				tree.roots.push_back(entity);
			}
		}

		// Whatever the roots don't reach sits in a Parent loop; take the lowest index of each as a root.
		std::unordered_set<Entity> reached;
		const auto reach = [&](Entity start)
		{
			std::vector<Entity> pending{start};
			while (!pending.empty())
			{
				const Entity entity = pending.back();
				pending.pop_back();
				if (!reached.insert(entity).second) continue;
				if (const auto found = tree.children.find(entity); found != tree.children.end())
				{
					pending.insert(pending.end(), found->second.begin(), found->second.end());
				}
			}
		};
		for (const Entity root : tree.roots)
			reach(root);
		for (const Entity entity : tree.all)
		{
			if (reached.contains(entity)) continue;
			tree.roots.push_back(entity);
			reach(entity);
		}
		return tree;
	}

	bool MatchesFilter(const Registry& registry, Entity entity, const std::string& filter)
	{
		if (filter.empty()) return true;
		const auto lower = [](std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});
			return text;
		};
		return lower(LabelOf(registry, entity)).find(lower(filter)) != std::string::npos;
	}

	bool SubtreeMatches(const Registry& registry, const Tree& tree, Entity entity, const std::string& filter)
	{
		if (filter.empty()) return true;
		std::unordered_set<Entity> seen; // a loop cannot run this forever
		std::vector<Entity> pending{entity};
		while (!pending.empty())
		{
			const Entity current = pending.back();
			pending.pop_back();
			if (!seen.insert(current).second) continue;
			if (MatchesFilter(registry, current, filter)) return true;
			if (const auto found = tree.children.find(current); found != tree.children.end())
			{
				pending.insert(pending.end(), found->second.begin(), found->second.end());
			}
		}
		return false;
	}
} // namespace engine::editor
