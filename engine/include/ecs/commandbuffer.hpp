#pragma once
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "core/assert.hpp"
#include "ecs/component.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"

namespace engine::ecs
{
	// Records structural changes (add/remove components, destroy entities) so they can be
	// applied later, at a point where no Registry::Each loop is iterating the pools.
	//
	//     CommandBuffer commands;
	//     registry.Each<Pellet, GridPosition>([&](Entity e, Pellet&, GridPosition& p)
	//     {
	//         if (p == playerTile) commands.Destroy(e);
	//     });
	//     commands.Apply(registry);
	//
	// Commands run in the order they were recorded. A command whose entity is no longer
	// alive when Apply runs (e.g. it was destroyed by an earlier command) is skipped.
	//
	// Registry::Create() is safe to call during iteration (it doesn't touch any pool), so
	// create entities directly and record their components here.
	class CommandBuffer
	{
		enum class Operation
		{
			Invalid,
			Add,
			Remove,
			Destroy
		};

		// Type-erased Add/Remove for a specific component type T.
		struct IComponentCommand
		{
			IComponentCommand() = default;
			IComponentCommand(const IComponentCommand&) = delete;
			IComponentCommand& operator=(const IComponentCommand&) = delete;
			IComponentCommand(IComponentCommand&&) = delete;
			IComponentCommand& operator=(IComponentCommand&&) = delete;
			virtual ~IComponentCommand() = default;

			virtual void Apply(Registry& registry, Entity entity) = 0;
		};

		// Holds the fully constructed component until Apply moves it into the registry.
		template <Component T> struct AddCommand final : IComponentCommand
		{
			T component;

			template <typename... Args> explicit AddCommand(Args&&... args) : component(std::forward<Args>(args)...) {}

			void Apply(Registry& registry, Entity entity) override { registry.Add<T>(entity, std::move(component)); }
		};

		template <Component T> struct RemoveCommand final : IComponentCommand
		{
			void Apply(Registry& registry, Entity entity) override { registry.Remove<T>(entity); }
		};

		struct Command
		{
			Operation op = Operation::Invalid;
			Entity entity;
			std::unique_ptr<IComponentCommand> component; // set for Add and Remove, null for Destroy
		};

		std::vector<Command> commands;

		void Push(Operation op, Entity entity, std::unique_ptr<IComponentCommand> component = nullptr)
		{
			commands.push_back(Command{op, entity, std::move(component)});
		}

	public:
		// Records Add<T>. The component is constructed now from args and moved into the
		// registry when Apply runs.
		template <Component T, typename... Args> void Add(Entity entity, Args&&... args)
		{
			Push(Operation::Add, entity, std::make_unique<AddCommand<T>>(std::forward<Args>(args)...));
		}

		template <Component T> void Remove(Entity entity)
		{
			Push(Operation::Remove, entity, std::make_unique<RemoveCommand<T>>());
		}

		void Destroy(Entity entity) { Push(Operation::Destroy, entity); }

		// Runs every recorded command against registry, then empties the buffer.
		// Must not be called from inside a Registry::Each callback.
		void Apply(Registry& registry)
		{
			for (Command& command : commands)
			{
				if (!registry.IsAlive(command.entity)) continue;

				switch (command.op)
				{
				case Operation::Add:
				case Operation::Remove:
					command.component->Apply(registry, command.entity);
					break;
				case Operation::Destroy:
					registry.Destroy(command.entity);
					break;
				case Operation::Invalid:
					ENGINE_ASSERT(false, "CommandBuffer recorded an invalid command");
					break;
				}
			}

			commands.clear();
		}

		// Drops every recorded command without applying it.
		void Clear() noexcept { commands.clear(); }

		bool Empty() const noexcept { return commands.empty(); }
		std::size_t Size() const noexcept { return commands.size(); }
	};
} // namespace engine::ecs
