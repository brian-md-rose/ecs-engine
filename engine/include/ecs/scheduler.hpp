#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ecs/world.hpp"

namespace engine::audio
{
	class AudioDevice;
}

namespace engine::input
{
	class Keyboard;
}

namespace engine::graphics
{
	class Renderer;
}

namespace engine::ecs
{
	// When a system runs. Input, Update and LateUpdate run at the fixed update rate, in that
	// order, zero or more times per frame; Render runs once per frame.
	enum class Phase : std::uint8_t
	{
		Input,      // read devices and toggle game state
		Update,     // game logic
		LateUpdate, // fix-ups that need everything moved: clamping, collisions
		Render,     // draw; must not change game state
		Count
	};

	inline constexpr std::size_t kPhaseCount = static_cast<std::size_t>(Phase::Count);

	class Scheduler;

	// What a system gets besides the World. Lent for one call and never stored, so nothing
	// here can dangle. Fields that don't apply to the running phase are zero or null.
	struct FrameContext
	{
		double dt = 0;                             // fixed phases: seconds per update
		double alpha = 0;                          // Render: how far into the next update, [0, 1)
		const input::Keyboard* keyboard = nullptr; // fixed phases
		graphics::Renderer* renderer = nullptr;    // Render
		audio::AudioDevice* audio = nullptr;       // fixed phases; null when there is no sound
		const Scheduler* scheduler = nullptr;      // the scheduler running the system (Scheduler::Run sets it)
	};

	// Systems are plain functions: state they keep between runs belongs in a resource.
	using System = void (*)(World& world, const FrameContext& context);

	// Lets a system be skipped, e.g. while paused. Must not change the world.
	using RunCondition = bool (*)(const World& world);

	// Holds systems by phase and runs them in the order they were added. After each phase it
	// applies world.commands, delivers world.events, then applies the commands the event
	// handlers queued, so everything one phase did is settled before the next begins. While a
	// phase runs (handlers included), adding or removing resources asserts.
	//
	// Every system run is timed. Timings() gives each system's smoothed time, for diagnostics
	// such as the debug panel.
	class Scheduler
	{
		struct Entry
		{
			System system;
			RunCondition runIf;
			std::string name;
			double averageMs = 0.0; // smoothed over recent runs
			bool timed = false;     // has run at least once
			bool skipped = false;   // runIf was false last time
		};

		std::array<std::vector<Entry>, kPhaseCount> phases;
		bool running = false;

	public:
		// How much each new run moves a system's average time: about the last 20 runs count.
		static constexpr double kTimingSmoothing = 0.05;

		// Appends system to phase. With runIf, the system is skipped whenever runIf is false. The
		// name is what diagnostics show; without one, a system is shown as "Phase #index".
		void Add(Phase phase, System system, RunCondition runIf = nullptr);
		void Add(Phase phase, std::string_view name, System system, RunCondition runIf = nullptr);

		// Runs phase's systems in order, then applies world.commands and delivers world.events
		// (see above). Not re-entrant. Each system gets context with scheduler set to this.
		void Run(Phase phase, World& world, const FrameContext& context);

		[[nodiscard]] std::size_t SystemCount(Phase phase) const;

		// One system's timing.
		struct SystemTiming
		{
			Phase phase = Phase::Update;
			std::string_view name;
			double averageMs = 0.0; // smoothed time per run, in milliseconds; 0 if it never ran
			bool skipped = false;   // its run condition was false the last time its phase ran
		};

		// Every system, phase by phase in run order.
		[[nodiscard]] std::vector<SystemTiming> Timings() const;
	};

	// "Input", "Update", "LateUpdate" or "Render".
	[[nodiscard]] std::string_view PhaseName(Phase phase) noexcept;
} // namespace engine::ecs
