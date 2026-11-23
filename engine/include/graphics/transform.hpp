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

	// How much an entity is stretched along x and y, 1 being its own size. A negative value mirrors it.
	struct Scale
	{
		float x = 1;
		float y = 1;
	};

	// Where an entity is, and how it is turned and stretched about that point. Shapes and sprites
	// are drawn with position as their top-left corner, and rotate and scale about it: the corner
	// stays put, and the rest swings and stretches around it. Rotation is in degrees, clockwise
	// on screen (y is down). Text ignores rotation and scale. The defaults (0 and 1) draw an
	// entity exactly as it was drawn before they existed.
	struct Transform
	{
		Position position{};
		float rotation = 0;
		Scale scale{};
	};

	// Component: where an entity is relative to its ecs::Parent. With both, the entity's Transform
	// is worked out from them: this position, stretched and turned by the parent's scale and
	// rotation, added to the parent's position; the rotations add and the scales multiply. Set the
	// parent's Transform, and its children follow. A child of a parent with a non-uniform scale and a
	// rotation is not sheared: its own axes just follow the combined scale and rotation.
	//
	// UpdateWorldTransforms does this; ScheduledGame calls it after every update and before
	// drawing. A child of a parent drawn with Interpolation needs an Interpolation of its own to
	// slide along with it.
	struct LocalTransform
	{
		Position position{};
		float rotation = 0;
		Scale scale{};
	};

	// Where offset (in the frame of a thing at origin, turned by rotation degrees and stretched by
	// scale) is: origin + rotate(scale * offset).
	[[nodiscard]] Position TransformPoint(Position origin, float rotation, Scale scale, Position offset) noexcept;

	// Sets the Transform of every entity with a Parent, a LocalTransform and a Transform from its
	// parent's. Parents of parents are worked out first, whatever order the entities are in. A
	// child whose parent is dead, or has no Transform, is left where it is.
	void UpdateWorldTransforms(ecs::Registry& registry);
} // namespace engine::graphics
