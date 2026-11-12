#pragma once
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "core/assert.hpp"
#include "ecs/component.hpp"
#include "ecs/componentpool.hpp"

namespace engine::ecs
{
	class Registry
	{
		std::vector<std::uint32_t> generations;
		std::vector<std::uint32_t> freeEntities;
		std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools;

		// Number of Each() calls currently running (they can nest). While it is non-zero,
		// structural changes (Add/Remove/Destroy) would invalidate the loop, so they assert.
		// The count is a single increment per Each() call; only the check is Debug-only.
		std::uint32_t iterationDepth = 0;

		// Increments iterationDepth for the lifetime of an Each() call, and decrements it
		// again even if the callback throws.
		class IterationScope
		{
			std::uint32_t& depth;

		public:
			explicit IterationScope(std::uint32_t& d) noexcept : depth(d) { ++depth; }
			~IterationScope() { --depth; }
			IterationScope(const IterationScope&) = delete;
			IterationScope& operator=(const IterationScope&) = delete;
		};

		void AssertNotIterating() const
		{
			ENGINE_ASSERT(iterationDepth == 0,
			              "structural change during Registry::Each; record it in a CommandBuffer instead");
		}

		template <Component T> ComponentPool<T>& Pool()
		{
			auto& p = pools[typeid(T)];
			if (!p) p = std::make_unique<ComponentPool<T>>();
			return static_cast<ComponentPool<T>&>(*p);
		}

	public:
		Entity Create();
		void Destroy(Entity entity);
		bool IsAlive(Entity entity) const;

		template <Component T, typename... Args> T& Add(Entity entity, Args&&... args)
		{
			AssertNotIterating();
			return Pool<T>().Add(entity, std::forward<Args>(args)...);
		}

		template <Component T> void Remove(Entity e)
		{
			AssertNotIterating();
			Pool<T>().Remove(e);
		}

		template <Component T> T& GetComponent(Entity e) { return Pool<T>().GetComponent(e); }

		template <Component T> bool HasComponent(Entity e) { return Pool<T>().HasComponent(e); }

		// Calls f(entity, First&, Rest&...) for every entity that has all the listed components.
		// Inside f, reading and modifying component values and calling Create() are fine;
		// Add/Remove/Destroy assert in Debug. Record those in a CommandBuffer instead.
		template <Component First, typename... Rest, typename Func> void Each(Func&& f)
		{
			const IterationScope scope(iterationDepth);
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
} // namespace engine::ecs