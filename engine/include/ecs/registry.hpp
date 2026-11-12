#pragma once
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <vector>

#include "ecs/component.hpp"
#include "ecs/componentpool.hpp"

namespace engine::ecs
{
	class Registry
	{
		std::vector<std::uint32_t> generations;
		std::vector<std::uint32_t> freeEntities;
		std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools;

		template<Component T>
		ComponentPool<T>& Pool()
		{
			auto& p = pools[typeid(T)];
			if (!p) p = std::make_unique<ComponentPool<T>>();
			return static_cast<ComponentPool<T>&>(*p);
		}

	public:
		Entity Create();
		void Destroy(Entity entity);
		bool IsAlive(Entity entity) const;

		template<Component T, typename... Args>
		T& Add(Entity entity, Args&&... args)
		{
			return Pool<T>().Add(entity, std::forward<Args>(args)...);
		}

		template<Component T>
		void Remove(Entity e)
		{
			Pool<T>().Remove(e);
		}

		template<Component T> T& GetComponent(Entity e) { return Pool<T>().GetComponent(e); }

		template<Component T> bool HasComponent(Entity e) { return Pool<T>().HasComponent(e); }

		// Calls f(entity, First&, Rest&...) for every entity that has all the listed components.
		template<Component First, typename... Rest, typename Func>
		void Each(Func&& f)
		{
			auto& first = Pool<First>();
			[&](auto&... rest) // look up the pools once, not per entity
			{
				auto ents = first.Entities();
				auto data = first.Data();
				for (std::size_t i = 0; i < ents.size(); ++i)
				{
					if ((rest.HasComponent(ents[i]) && ...))
					{
						f(ents[i], data[i], rest.GetComponent(ents[i])...);
					}
				}
			}(Pool<Rest>()...);
		}
	};
}