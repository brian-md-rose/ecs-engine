#pragma once
#include <string>

namespace engine::ecs
{
	// Component: on the root of an entity tree that the editor made from a prefab, the name of the prefab (the
	// file `<name>.json` in the assets' prefabs folder). A game ignores it; the editor uses it to apply the tree back
	// to the prefab or to revert it.
	struct PrefabInstance
	{
		std::string name;
	};
} // namespace engine::ecs
