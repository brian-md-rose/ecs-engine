#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "ecs/world.hpp"

namespace engine::audio
{
	class AudioDevice;
}

namespace engine::input
{
	struct Input;
}

namespace engine::graphics
{
	class Renderer;
}

namespace engine::ecs
{
	// What a game state is: an enum (Screen::Title, Round::Ready). See states.hpp.
	template <typename T>
	concept StateValue = std::is_enum_v<T>;

	namespace detail
	{
		class IStateMachine;
	}

	// When a system runs. Transition runs first, once per update, and holds the enter and exit
	// systems of game states (see states.hpp); they are added with AddOnEnter and AddOnExit, not
	// Add. Input, Update and LateUpdate then run at the fixed update rate, in that order, zero
	// or more times per frame; Render runs once per frame.
	enum class Phase : std::uint8_t
	{
		Transition, // apply requested state changes: exit systems, then enter systems
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
		double dt = 0;                          // fixed phases: seconds per update
		double alpha = 0;                       // Render: how far into the next update, [0, 1)
		const input::Input* input = nullptr;    // fixed phases
		graphics::Renderer* renderer = nullptr; // Render
		audio::AudioDevice* audio = nullptr;    // fixed phases; null when there is no sound
		const Scheduler* scheduler = nullptr;   // the scheduler running the system (Scheduler::Run sets it)
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

		// Game states (see states.hpp): every machine, the ones with no parent, and each by type.
		std::vector<std::unique_ptr<detail::IStateMachine>> machines;
		std::vector<detail::IStateMachine*> roots;
		std::unordered_map<std::type_index, detail::IStateMachine*> machineOf;

	public:
		// How much each new run moves a system's average time: about the last 20 runs count.
		static constexpr double kTimingSmoothing = 0.05;

		Scheduler();
		~Scheduler();

		Scheduler(const Scheduler&) = delete;
		Scheduler& operator=(const Scheduler&) = delete;
		Scheduler(Scheduler&&) = delete;
		Scheduler& operator=(Scheduler&&) = delete;

		// The next smoothed time after a run of tookMs. The first run (timed false) is taken as is.
		[[nodiscard]] static double NextAverage(double averageMs, bool timed, double tookMs) noexcept;

		// Appends system to phase. With runIf, the system is skipped whenever runIf is false. The
		// name is what diagnostics show; without one, a system is shown as "Phase #index".
		void Add(Phase phase, System system, RunCondition runIf = nullptr);
		void Add(Phase phase, std::string_view name, System system, RunCondition runIf = nullptr);

		// Game states. Each needs its State<T> and NextState<T> resources, which AddState and
		// AddSubState add to world. All of these are for setup, before the first update.

		// A state machine over T, starting in initial. Its enter systems for initial run at the
		// first RunTransitions.
		template <StateValue T> void AddState(World& world, T initial);

		// A state machine over Sub that exists only while Parent's machine is in parentValue: it
		// starts in initial each time the parent enters parentValue, and its exit systems run
		// when the parent leaves it. Add the parent's AddState first.
		template <StateValue Sub, StateValue Parent> void AddSubState(World& world, Parent parentValue, Sub initial);

		// Runs system when T's state is entered (in the update after it was requested), after
		// the exit systems of the state it left. The name is what diagnostics show.
		template <StateValue T> void AddOnEnter(T value, std::string_view name, System system);

		// Runs system when T's state is left, before the new state's enter systems; entities
		// with DespawnOnExit are removed after it.
		template <StateValue T> void AddOnExit(T value, std::string_view name, System system);

		// Applies every state change requested through NextState since the last call: exit
		// systems of the old value, then enter systems of the new one, parents before their
		// sub-states on the way in and after them on the way out. Then applies world.commands
		// and delivers world.events, like a phase. ScheduledGame::Update calls it first, so a
		// change requested during one update takes effect at the start of the next.
		void RunTransitions(World& world, const FrameContext& context);

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

		// Every system, phase by phase in run order; the enter and exit systems of states come first.
		[[nodiscard]] std::vector<SystemTiming> Timings() const;
	};

	// "Input", "Update", "LateUpdate" or "Render".
	[[nodiscard]] std::string_view PhaseName(Phase phase) noexcept;
} // namespace engine::ecs

// The templates declared above are defined here.
#include "ecs/states.hpp"
