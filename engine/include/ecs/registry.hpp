#pragma once
#include <cstddef>
#include <memory>
#include <span>
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

		template <Component T> ComponentPool<T>* FindPool()
		{
			const auto it = pools.find(std::type_index(typeid(T)));
			if (it == pools.end())
			{
				return nullptr;
			}
			// Safe: the entry for typeid(T) is only ever created by Pool<T>() as a ComponentPool<T>.
			return static_cast<ComponentPool<T>*>(it->second.get());
		}

		template <Component T> const ComponentPool<T>* FindPool() const
		{
			const auto it = pools.find(std::type_index(typeid(T)));
			if (it == pools.end())
			{
				return nullptr;
			}
			return static_cast<const ComponentPool<T>*>(it->second.get());
		}

	public:
		Entity Create();
		void Destroy(Entity entity);
		bool IsAlive(Entity entity) const;

		template <Component T, typename... Args> T& Add(Entity entity, Args&&... args)
		{
			AssertNotIterating();
			// Always on: a dead or null entity would leave an orphaned entry in the pool
			// (or, for Entity::Null, write out of bounds).
			ENGINE_VERIFY(IsAlive(entity), "cannot add a component to a dead or null entity");
			return Pool<T>().Add(entity, std::forward<Args>(args)...);
		}

		// Removing from a dead or null entity, or one without the component, is a deliberate no-op
		// rather than an assert: a destroyed entity already has no components, so the request is
		// already satisfied, and deferred removals (CommandBuffer) can race with Destroy.
		template <Component T> void Remove(Entity entity)
		{
			AssertNotIterating();
			ComponentPool<T>* pool = FindPool<T>();
			if (pool != nullptr)
			{
				pool->Remove(entity);
			}
		}

		template <Component T> T& GetComponent(Entity entity)
		{
			ComponentPool<T>* pool = FindPool<T>();
			ENGINE_ASSERT(pool != nullptr && pool->HasComponent(entity), "entity does not have this component");
			return pool->GetComponent(entity);
		}

		template <Component T> const T& GetComponent(Entity entity) const
		{
			const ComponentPool<T>* pool = FindPool<T>();
			ENGINE_ASSERT(pool != nullptr && pool->HasComponent(entity), "entity does not have this component");
			return pool->GetComponent(entity);
		}

		template <Component T> T* TryGetComponent(Entity entity)
		{
			ComponentPool<T>* pool = FindPool<T>();
			return pool != nullptr && pool->HasComponent(entity) ? &pool->GetComponent(entity) : nullptr;
		}

		template <Component T> const T* TryGetComponent(Entity entity) const
		{
			const ComponentPool<T>* pool = FindPool<T>();
			return pool != nullptr && pool->HasComponent(entity) ? &pool->GetComponent(entity) : nullptr;
		}

		template <Component T> bool HasComponent(Entity entity) const
		{
			const ComponentPool<T>* pool = FindPool<T>();
			return pool != nullptr && pool->HasComponent(entity);
		}

		// Calls f(entity, First&, Rest&...) for every entity that has all the listed components.
		// Inside f, reading and modifying component values and calling Create() are fine;
		// Add/Remove/Destroy assert in Debug. Record those in a CommandBuffer instead.
		//
		// The loop is driven by whichever listed pool is smallest (e.g. Each<Position, Enemy>
		// walks the few enemies, not every positioned entity), so visiting order follows that
		// pool's packed order. The callback's arguments are always in the order written.
		template <Component First, typename... Rest, typename Func> void Each(Func&& f)
		{
			const IterationScope scope(iterationDepth);

			ComponentPool<First>* pool = FindPool<First>();
			if (pool == nullptr)
			{
				return;
			}

			auto& first = *pool;
			[&](auto... rest) // look up the pools once, not per entity
			{
				if (!(rest && ...)) return;

				const std::span<const Entity> ents = first.Entities();

				// Pick the smallest pool. Safe to hold the span: structural changes,
				// which could reallocate it, assert while iterating.
				std::span<const Entity> driver = ents;
				((driver = rest->Entities().size() < driver.size() ? rest->Entities() : driver), ...);

				if (driver.data() == ents.data())
				{
					// First is the smallest: walk its packed data directly, no lookup per entity.
					const std::span<First> data = first.Data();
					for (std::size_t i = 0; i < ents.size(); ++i)
					{
						if ((rest->HasComponent(ents[i]) && ...))
						{
							f(ents[i], data[i], rest->GetComponent(ents[i])...);
						}
					}
					return;
				}

				for (const Entity entity : driver)
				{
					if (first.HasComponent(entity) && (rest->HasComponent(entity) && ...))
					{
						f(entity, first.GetComponent(entity), rest->GetComponent(entity)...);
					}
				}
			}(FindPool<Rest>()...);
		}

		// Number of component pools that exist. Diagnostic; tests use it to prove that
		// read-only queries don't create pools.
		[[nodiscard]] std::size_t PoolCount() const noexcept { return pools.size(); }
	};
} // namespace engine::ecs
