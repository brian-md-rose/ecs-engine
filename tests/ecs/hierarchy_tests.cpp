#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "ecs/commandbuffer.hpp"
#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"

namespace
{
	using engine::ecs::CommandBuffer;
	using engine::ecs::Entity;
	using engine::ecs::Parent;
	using engine::ecs::Registry;

	bool Contains(const std::vector<Entity>& entities, Entity entity)
	{
		return std::ranges::find(entities, entity) != entities.end();
	}
} // namespace

TEST_CASE("ChildrenOf and DescendantsOf follow Parent links", "[ecs][hierarchy]")
{
	Registry registry;
	const Entity panel = registry.Create();
	const Entity label = registry.Create();
	const Entity icon = registry.Create();
	const Entity iconShadow = registry.Create();
	const Entity elsewhere = registry.Create();
	registry.Add<Parent>(label, Parent{panel});
	registry.Add<Parent>(icon, Parent{panel});
	registry.Add<Parent>(iconShadow, Parent{icon});

	const std::vector<Entity> children = ChildrenOf(registry, panel);
	CHECK(children.size() == 2);
	CHECK(Contains(children, label));
	CHECK(Contains(children, icon));

	const std::vector<Entity> all = DescendantsOf(registry, panel);
	CHECK(all.size() == 3);
	CHECK(Contains(all, iconShadow));
	CHECK_FALSE(Contains(all, elsewhere));
	CHECK_FALSE(Contains(all, panel));

	CHECK(ChildrenOf(registry, elsewhere).empty());
}

TEST_CASE("DestroyWithChildren records the whole tree and nothing else", "[ecs][hierarchy]")
{
	Registry registry;
	const Entity panel = registry.Create();
	const Entity label = registry.Create();
	const Entity grandchild = registry.Create();
	const Entity other = registry.Create();
	registry.Add<Parent>(label, Parent{panel});
	registry.Add<Parent>(grandchild, Parent{label});

	CommandBuffer commands;
	DestroyWithChildren(registry, commands, panel);
	CHECK(registry.IsAlive(panel)); // only recorded so far
	commands.Apply(registry);

	CHECK_FALSE(registry.IsAlive(panel));
	CHECK_FALSE(registry.IsAlive(label));
	CHECK_FALSE(registry.IsAlive(grandchild));
	CHECK(registry.IsAlive(other));
}
