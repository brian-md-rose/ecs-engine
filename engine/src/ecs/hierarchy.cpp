#include "ecs/hierarchy.hpp"

#include <cstddef>

#include "ecs/commandbuffer.hpp"
#include "ecs/registry.hpp"

namespace engine::ecs
{
	std::vector<Entity> ChildrenOf(Registry& registry, Entity parent)
	{
		std::vector<Entity> children;
		registry.Each<Parent>([&](Entity child, const Parent& link)
		{
			if (link.entity == parent) children.push_back(child);
		});
		return children;
	}

	std::vector<Entity> DescendantsOf(Registry& registry, Entity parent)
	{
		// Breadth first: each entity found is searched for children in turn. The Parent pool is
		// walked once per entity found, which is fine for the small trees of a HUD or a menu.
		std::vector<Entity> found = ChildrenOf(registry, parent);
		for (std::size_t i = 0; i < found.size(); ++i)
		{
			const std::vector<Entity> children = ChildrenOf(registry, found[i]);
			found.insert(found.end(), children.begin(), children.end());
		}
		return found;
	}

	void DestroyWithChildren(Registry& registry, CommandBuffer& commands, Entity entity)
	{
		for (const Entity descendant : DescendantsOf(registry, entity))
		{
			commands.Destroy(descendant);
		}
		commands.Destroy(entity);
	}
} // namespace engine::ecs
