#pragma once

namespace engine::ecs
{
	class Registry;
}

namespace engine::graphics
{
	// In logical pixels, origin top-left, y down.
	struct Position
	{
		float x = 0;
		float y = 0;
	};

	// Where an entity is. Room to add scale and rotation once something uses them.
	struct Transform
	{
		Position position{};
	};

	// Component: where an entity is relative to its ecs::Parent. With both, the entity's Transform
	// is worked out from them: its parent's Transform plus this position. Set the parent's
	// Transform, and its children follow.
	//
	// UpdateWorldTransforms does this; ScheduledGame calls it after every update and before
	// drawing. A child of a parent drawn with Interpolation needs an Interpolation of its own to
	// slide along with it.
	struct LocalTransform
	{
		Position position{};
	};

	// Sets the Transform of every entity with a Parent, a LocalTransform and a Transform from its
	// parent's. Parents of parents are worked out first, whatever order the entities are in. A
	// child whose parent is dead, or has no Transform, is left where it is.
	void UpdateWorldTransforms(ecs::Registry& registry);
} // namespace engine::graphics
