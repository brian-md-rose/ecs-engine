#pragma once

#include <vector>

#include "ecs/entity.hpp"

namespace engine::ecs
{
	class CommandBuffer;
	class Registry;

	// Component: this entity belongs to another, its parent. Parents can have parents of their own.
	// What belonging means is up to the systems that read it: graphics::LocalTransform places a
	// child relative to its parent, so a HUD panel, its text and its icons move together.
	//
	// A child whose parent has been destroyed keeps its Parent, pointing at a dead entity, until
	// it is destroyed too; DestroyWithChildren destroys a parent and everything under it.
	struct Parent
	{
		Entity entity = Entity::Null;
	};

	// The entities whose Parent is parent, in no particular order. Not their children.
	[[nodiscard]] std::vector<Entity> ChildrenOf(Registry& registry, Entity parent);

	// Every entity under parent: its children, their children and so on, parents before their
	// children. Not parent itself.
	[[nodiscard]] std::vector<Entity> DescendantsOf(Registry& registry, Entity parent);

	// Records the destruction of entity and everything under it in commands. Recorded, not done,
	// so a system can call it inside Each.
	void DestroyWithChildren(Registry& registry, CommandBuffer& commands, Entity entity);
} // namespace engine::ecs
