#pragma once

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
} // namespace engine::graphics
