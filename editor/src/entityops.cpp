#include "editor/entityops.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "core/json.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/name.hpp"

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
		for (std::size_t i = 0; i < originals.size(); ++i)
		{
			for (std::size_t t = 0; t < types.Count(); ++t)
			{
				const ecs::TypeInfo& type = types.At(t);
				if (type.IsRuntime() || !type.Has(registry, originals[i])) continue;
				core::JsonValue saved;
				const std::string path = LabelOf(registry, originals[i]) + "." + type.Name();
				if (!type.Save(registry, originals[i], ids, saved, issues, path)) continue;
				(void)type.Load(registry, copies[i], saved, ids, issues, path);
			}
		}

		if (issues.errors.size() != errorsBefore)
		{
			for (auto it = copies.rbegin(); it != copies.rend(); ++it)
				registry.Destroy(*it);
			return Entity::Null;
		}
		if (ecs::Name* name = registry.TryGetComponent<ecs::Name>(copies.front())) name->value += " (copy)";
		return copies.front();
	}
} // namespace engine::editor
