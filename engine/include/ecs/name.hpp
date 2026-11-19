#pragma once
#include <string>

namespace engine::ecs
{
	// Component: a name for an entity, shown in an editor's hierarchy and kept in saved scenes.
	// Optional; an entity without one is shown by its number.
	struct Name
	{
		std::string value;
	};
} // namespace engine::ecs
