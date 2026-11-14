#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ecs/world.hpp"

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

	// What a system gets besides the World. Lent for one call and never stored, so nothing
	// here can dangle. Fields that don't apply to the running phase are zero or null.
	struct FrameContext
	{
		double dt = 0;                             // fixed phases: seconds per update
		double alpha = 0;                          // Render: how far into the next update, [0, 1)
		const input::Keyboard* keyboard = nullptr; // fixed phases
		graphics::Renderer* renderer = nullptr;    // Render
	};

	// Systems are plain functions: state they keep between runs belongs in a resource.
	using System = void (*)(World& world, const FrameContext& context);

	// Lets a system be skipped, e.g. while paused. Must not change the world.
	using RunCondition = bool (*)(const World& world);

	// Holds systems by phase and runs them in the order they were added. After each phase it
	// applies world.commands, delivers world.events, then applies the commands the event
	// handlers queued, so everything one phase did is settled before the next begins. While a
	// phase runs (handlers included), adding or removing resources asserts.
	class Scheduler
	{
		struct Entry
		{
			System system;
			RunCondition runIf;
		};

		std::array<std::vector<Entry>, kPhaseCount> phases;
		bool running = false;

	public:
		// Appends system to phase. With runIf, the system is skipped whenever runIf is false.
		void Add(Phase phase, System system, RunCondition runIf = nullptr);

		// Runs phase's systems in order, then applies world.commands and delivers world.events
		// (see above). Not re-entrant.
		void Run(Phase phase, World& world, const FrameContext& context);

		[[nodiscard]] std::size_t SystemCount(Phase phase) const;
	};
} // namespace engine::ecs
