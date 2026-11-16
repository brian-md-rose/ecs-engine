#include "ecs/registry.hpp"

#include <algorithm>

#include "ecs/entity.hpp"

namespace engine::ecs
{
	Entity Registry::Create()
	{
		if (!freeEntities.empty())
		{
			std::uint32_t id = freeEntities.back();
			freeEntities.pop_back();
			return {id, generations[id]};
		}
		generations.push_back(0);

		// Returns new Entity with an id and generation
		return {static_cast<std::uint32_t>(generations.size() - 1), 0};
	}

	void Registry::Destroy(Entity entity)
	{
		AssertNotIterating();
		if (!IsAlive(entity)) return;

		// Every OnRemove hook first, while the entity still has all its components.
		{
			const IterationScope scope(iterationDepth);
			for (auto& [type, pool] : pools)
			{
				pool->RunRemoveHooks(entity);
			}
		}
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

	std::vector<Registry::ComponentCount> Registry::ComponentCounts() const
	{
		std::vector<ComponentCount> counts;
		counts.reserve(pools.size());
		for (const auto& [type, pool] : pools)
		{
			counts.push_back({.name = pool->Name(), .count = pool->Size()});
		}
		std::ranges::sort(counts, {}, &ComponentCount::name);
		return counts;
	}
} // namespace engine::ecs