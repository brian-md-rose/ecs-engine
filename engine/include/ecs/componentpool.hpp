#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.hpp"
#include "core/typename.hpp"
#include "ecs/component.hpp"
#include "ecs/entity.hpp"

namespace engine::ecs
{
	class IComponentPool
	{
	public:
		virtual ~IComponentPool() = default;
		virtual void Remove(Entity entity) = 0;

		// Calls the OnRemove hooks for entity, if it has this component and there are any. The
		// registry calls it just before the component is removed (see Registry::OnRemove).
		virtual void RunRemoveHooks(Entity entity) = 0;

		// How many entities have this component, and its name (core::TypeName), for diagnostics.
		[[nodiscard]] virtual std::size_t Size() const noexcept = 0;
		[[nodiscard]] virtual std::string_view Name() const noexcept = 0;
	};

	template <Component T> class ComponentPool final : public IComponentPool
	{
		static constexpr std::uint32_t npos = UINT32_MAX;
		std::vector<std::uint32_t> sparse; // entity id map to component index (ex: Entity Id(2) -> sparse[2] = 5 ->
		                                   // components[5] is for that entity
		std::vector<Entity> entities;      // entities that have a component of type T
		std::vector<T> components;         // all components of type T, tightly packed

	public:
		// Called with the entity and its component: just after it is added, or just before it is
		// removed. Registry::OnAdd and Registry::OnRemove register them.
		using Hook = std::function<void(Entity, T&)>;

	private:
		std::vector<Hook> addHooks;
		std::vector<Hook> removeHooks;

	public:
		void AddAddHook(Hook hook) { addHooks.push_back(std::move(hook)); }
		void AddRemoveHook(Hook hook) { removeHooks.push_back(std::move(hook)); }
		[[nodiscard]] bool HasAddHooks() const noexcept { return !addHooks.empty(); }

		// Calls the OnAdd hooks, in the order they were registered. The entity must have the component.
		void RunAddHooks(Entity entity)
		{
			for (const Hook& hook : addHooks)
			{
				hook(entity, GetComponent(entity));
			}
		}

		void RunRemoveHooks(Entity entity) override
		{
			if (removeHooks.empty() || !HasComponent(entity)) return;
			for (const Hook& hook : removeHooks)
			{
				hook(entity, GetComponent(entity));
			}
		}

		template <typename... Args> T& Add(Entity entity, Args&&... args)
		{
			// Always on: a null id would overflow the resize below and write out of bounds.
			ENGINE_VERIFY(!entity.IsNull(), "cannot add a component to null entity");
			ENGINE_ASSERT(!HasComponent(entity), "entity already has this component; use GetComponent to modify it");

			// Asserts are compiled out in Release: never store a second copy,
			// which would orphan the first and corrupt the sparse set.
			if (HasComponent(entity))
			{
				return GetComponent(entity);
			}

			// resize sparse to handle more entities
			if (entity.id >= sparse.size())
			{
				const std::uint32_t newSparseSize = entity.id + 1;
				sparse.resize(newSparseSize, npos);
			}

			sparse[entity.id] = static_cast<std::uint32_t>(components.size());

			entities.push_back(entity);

			return components.emplace_back(std::forward<Args>(args)...);
		}

		void Remove(Entity entity) override // swap-and-pop keeps the array packed
		{
			if (!HasComponent(entity)) return;

			const std::uint32_t slot = sparse[entity.id];
			const std::uint32_t last = static_cast<std::uint32_t>(components.size() - 1);

			// Only swap when the removed element isn't already at the back;
			// otherwise this would move-assign an object onto itself.
			if (slot != last)
			{
				components[slot] = std::move(components[last]);
				entities[slot] = entities[last];
				sparse[entities[slot].id] = slot;
			}

			components.pop_back();
			entities.pop_back();
			sparse[entity.id] = npos;
		}

		bool HasComponent(Entity entity) const
		{
			return entity.id < sparse.size() && sparse[entity.id] != npos && entities[sparse[entity.id]] == entity;
		}

		// The entity must have the component (check with HasComponent first).
		T& GetComponent(Entity entity)
		{
			ENGINE_ASSERT(HasComponent(entity), "entity does not have this component");
			return components[sparse[entity.id]];
		}

		const T& GetComponent(Entity entity) const
		{
			ENGINE_ASSERT(HasComponent(entity), "entity does not have this component");
			return components[sparse[entity.id]];
		}

		// std::span is a lightweight, non-owning view over a contiguous sequence of memory.
		// It wraps a raw pointer and a size into a single, cohesive type, acting as a universal interface for functions
		// that process linear blocks of data.
		std::span<T> Data() { return components; }
		std::span<const Entity> Entities() const { return entities; }

		[[nodiscard]] std::size_t Size() const noexcept override { return components.size(); }
		[[nodiscard]] std::string_view Name() const noexcept override { return core::TypeName<T>(); }
	};
} // namespace engine::ecs
