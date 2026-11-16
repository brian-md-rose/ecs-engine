#include "ecs/scheduler.hpp"

#include <chrono>
#include <string>

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
		const std::size_t index = phases[Index(phase)].size();
		Add(phase, std::string(PhaseName(phase)) + " #" + std::to_string(index), system, runIf);
	}

	void Scheduler::Add(Phase phase, std::string_view name, System system, RunCondition runIf)
	{
		ENGINE_VERIFY(system != nullptr, "cannot add a null system");
		ENGINE_ASSERT(!running, "cannot add systems while a phase is running");
		phases[Index(phase)].push_back({.system = system, .runIf = runIf, .name = std::string(name)});
	}

	void Scheduler::Run(Phase phase, World& world, const FrameContext& context)
	{
		ENGINE_ASSERT(!running, "Scheduler::Run is not re-entrant");
		FrameContext systemContext = context;
		systemContext.scheduler = this;
		{
			const FlagScope schedulerRunning(running);
			const Resources::LockScope resourcesLocked(world.resources);
			for (Entry& entry : phases[Index(phase)])
			{
				entry.skipped = entry.runIf != nullptr && !entry.runIf(world);
				if (entry.skipped) continue;

				const auto start = std::chrono::steady_clock::now();
				entry.system(world, systemContext);
				const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - start;
				entry.averageMs =
				    entry.timed ? entry.averageMs + (took.count() - entry.averageMs) * kTimingSmoothing : took.count();
				entry.timed = true;
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

	std::vector<Scheduler::SystemTiming> Scheduler::Timings() const
	{
		std::vector<SystemTiming> timings;
		for (std::size_t p = 0; p < kPhaseCount; ++p)
		{
			for (const Entry& entry : phases[p])
			{
				timings.push_back({.phase = static_cast<Phase>(p),
				                   .name = entry.name,
				                   .averageMs = entry.averageMs,
				                   .skipped = entry.skipped});
			}
		}
		return timings;
	}

	std::string_view PhaseName(Phase phase) noexcept
	{
		switch (phase)
		{
		case Phase::Input:
			return "Input";
		case Phase::Update:
			return "Update";
		case Phase::LateUpdate:
			return "LateUpdate";
		case Phase::Render:
			return "Render";
		case Phase::Count:
			break;
		}
		return "?";
	}
} // namespace engine::ecs
