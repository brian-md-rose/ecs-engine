#include "ecs/registry.hpp"
#include "ecs/entity.hpp"

namespace engine::ecs
{
	Entity Registry::Create()
	{
		if (!freeEntities.empty())
		{
			std::uint32_t id = freeEntities.back(); 
			freeEntities.pop_back();
			return { id, generations[id] };
		}
		generations.push_back(0);

		// Returns new Entity with an id and generation
		return { static_cast<std::uint32_t>(generations.size() - 1), 0 };
	}

	void Registry::Destroy(Entity entity)
	{
		if (!IsAlive(entity)) return;

		for (auto& [type, pool] : pools)
		{
			pool->Remove(entity);
		}

		++generations[entity.id];
		
		freeEntities.push_back(entity.id);
	}

	bool Registry::IsAlive(Entity entity) const
	{
		return entity.id < generations.size() && generations[entity.id] == entity.generation;
	}
}