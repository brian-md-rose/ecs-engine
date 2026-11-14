#include "ecs/scheduler.hpp"

#include "core/assert.hpp"

namespace engine::ecs
{
	namespace
	{
		std::size_t Index(Phase phase)
		{
			const auto index = static_cast<std::size_t>(phase);
			ENGINE_ASSERT(index < kPhaseCount, "invalid phase");
			return index;
		}

		// Sets a flag for the lifetime of the scope and clears it again, even if a system
		// throws (as a failed assert does in tests).
		class FlagScope
		{
			bool& flag;

		public:
			explicit FlagScope(bool& f) noexcept : flag(f) { flag = true; }
			~FlagScope() { flag = false; }

			FlagScope(const FlagScope&) = delete;
			FlagScope& operator=(const FlagScope&) = delete;
			FlagScope(FlagScope&&) = delete;
			FlagScope& operator=(FlagScope&&) = delete;
		};
	} // namespace

	void Scheduler::Add(Phase phase, System system, RunCondition runIf)
	{
		ENGINE_VERIFY(system != nullptr, "cannot add a null system");
		ENGINE_ASSERT(!running, "cannot add systems while a phase is running");
		phases[Index(phase)].push_back({system, runIf});
	}

	void Scheduler::Run(Phase phase, World& world, const FrameContext& context)
	{
		ENGINE_ASSERT(!running, "Scheduler::Run is not re-entrant");
		{
			const FlagScope schedulerRunning(running);
			const Resources::LockScope resourcesLocked(world.resources);
			for (const Entry& entry : phases[Index(phase)])
			{
				if (entry.runIf == nullptr || entry.runIf(world))
				{
					entry.system(world, context);
				}
			}

			// Handlers see the entities the phase created and destroyed, and may queue
			// commands of their own, applied below. Resources stay locked for them too.
			world.commands.Apply(world.registry);
			world.events.Deliver(world);
		}
		world.commands.Apply(world.registry);
	}

	std::size_t Scheduler::SystemCount(Phase phase) const
	{
		return phases[Index(phase)].size();
	}
} // namespace engine::ecs
