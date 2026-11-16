#pragma once
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

#include "core/assert.hpp"
#include "ecs/entity.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"

// Game states, modelled on Bevy's: a game is always in one value of an enum (the title screen,
// the options screen, playing), and the systems that belong to a value run only while it is
// current.
//
//     enum class Screen { Title, Playing };
//
//     scheduler.AddState(world, Screen::Title);                       // setup
//     scheduler.AddOnEnter(Screen::Title, "ShowTitle", ShowTitle);    // spawn the menu...
//     scheduler.AddOnExit(Screen::Title, "StopTitle", StopTitle);
//     scheduler.Add(Phase::Update, "Move", Move, InState<Screen::Playing>);
//
//     world.resources.Get<NextState<Screen>>().Set(Screen::Playing);  // in a system
//
// A system never changes the state itself: it asks through NextState<T>. The request is applied
// once per update, before Input (Scheduler::RunTransitions, called by ScheduledGame::Update):
// the old value's exit systems run, then the new value's enter systems. A change requested
// during a phase, or by an enter or exit system, therefore takes effect at the start of the
// next update, and every system within one update sees one state. Asking for the value the
// state already has does nothing: no exit, no enter.
//
// The first RunTransitions runs the enter systems of the initial value.
//
// A sub-state (AddSubState) exists only while its parent is in one value, like "Paused" while
// "Playing". It starts again from its initial value on every entry to the parent value, and
// while the parent is elsewhere InState is false for all its values.
namespace engine::ecs
{
	// Resource: the current value of the state machine over T. Read it; change it through
	// NextState.
	template <StateValue T> struct State
	{
		T value{};
		bool active = true; // false for a sub-state while its parent is not in the value it needs

		State() = default;
		State(T initial, bool isActive) noexcept : value(initial), active(isActive) {}

		// True if the machine is running and in v.
		[[nodiscard]] bool Is(T v) const noexcept { return active && value == v; }
	};

	// Resource: the change asked for, applied at the start of the next update.
	template <StateValue T> struct NextState
	{
		std::optional<T> requested;

		// Asks for v. The last request before the update wins.
		void Set(T v) noexcept { requested = v; }
		void Clear() noexcept { requested.reset(); }
	};

	// Component: destroy the entity when T's state leaves value, after the exit systems have
	// run. This is how a screen's menu or text goes away without anyone hiding it.
	template <StateValue T> struct DespawnOnExit
	{
		T value{};
	};

	// Component: destroy the entity when T's state enters value, before the enter systems run.
	// Before, so that what an enter system spawns with the same scope is not destroyed by the entry
	// that spawned it. This is how a world is cleared away on return to a menu: the entities that
	// belong to the game carry DespawnOnEnter<Screen>{Screen::Title}.
	template <StateValue T> struct DespawnOnEnter
	{
		T value{};
	};

	// Run conditions. Pass them to Scheduler::Add with the value as a template argument:
	//
	//     scheduler.Add(Phase::Update, "Move", Move, InState<Screen::Playing>);
	//
	// InState is true while T's machine is in any of the values; NotInState is its opposite, so
	// it is also true while a sub-state is inactive.
	template <auto First, auto... Rest>
	    requires(StateValue<decltype(First)> && (std::is_same_v<decltype(First), decltype(Rest)> && ...))
	[[nodiscard]] bool InState(const World& world)
	{
		const auto& state = world.resources.Get<State<decltype(First)>>();
		return state.Is(First) || (state.Is(Rest) || ...);
	}

	template <auto First, auto... Rest>
	    requires(StateValue<decltype(First)> && (std::is_same_v<decltype(First), decltype(Rest)> && ...))
	[[nodiscard]] bool NotInState(const World& world)
	{
		return !InState<First, Rest...>(world);
	}

	namespace detail
	{
		// An enter or exit system with its timing.
		struct Hook
		{
			System system = nullptr;
			std::string name;
			double averageMs = 0.0;
			bool timed = false;
			bool skipped = true; // did not run in the last RunTransitions
		};

		inline void RunHook(Hook& hook, World& world, const FrameContext& context)
		{
			const auto start = std::chrono::steady_clock::now();
			hook.system(world, context);
			const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - start;
			hook.averageMs = Scheduler::NextAverage(hook.averageMs, hook.timed, took.count());
			hook.timed = true;
			hook.skipped = false;
		}

		// A state machine with its type erased, so the Scheduler can hold them all.
		class IStateMachine
		{
		public:
			IStateMachine() = default;
			IStateMachine(const IStateMachine&) = delete;
			IStateMachine& operator=(const IStateMachine&) = delete;
			IStateMachine(IStateMachine&&) = delete;
			IStateMachine& operator=(IStateMachine&&) = delete;
			virtual ~IStateMachine() = default;

			// Runs the initial value's enter systems the first time, then applies a requested
			// change. The machine's sub-states are processed after it.
			virtual void Process(World& world, const FrameContext& context) = 0;

			// Sub-states: start in the initial value, or stop, when the parent moves.
			virtual void Activate(World& world, const FrameContext& context) = 0;
			virtual void Deactivate(World& world, const FrameContext& context) = 0;

			virtual void BeginPass() noexcept = 0;
			virtual void AppendTimings(std::vector<Scheduler::SystemTiming>& out) const = 0;
		};

		template <StateValue T> class StateMachine final : public IStateMachine
		{
			struct Child
			{
				T parentValue;
				IStateMachine* machine;
			};

			T initial;
			bool started = false;
			std::map<T, std::vector<Hook>> enter;
			std::map<T, std::vector<Hook>> exit;
			std::vector<Child> children;
			State<T>* state;
			NextState<T>* next;

			void RunHooks(std::vector<Hook>* hooks, World& world, const FrameContext& context)
			{
				if (hooks == nullptr) return;
				for (Hook& hook : *hooks)
				{
					RunHook(hook, world, context);
					world.commands.Apply(world.registry); // what it created is there for the next
				}
			}

			[[nodiscard]] static std::vector<Hook>* HooksOf(std::map<T, std::vector<Hook>>& hooks, T value)
			{
				const auto it = hooks.find(value);
				return it == hooks.end() ? nullptr : &it->second;
			}

			void Enter(T value, World& world, const FrameContext& context)
			{
				world.registry.template Each<DespawnOnEnter<T>>([&](Entity entity, const DespawnOnEnter<T>& scope)
				{
					if (scope.value == value) world.commands.Destroy(entity);
				});
				world.commands.Apply(world.registry);
				RunHooks(HooksOf(enter, value), world, context);
				for (const Child& child : children)
				{
					if (child.parentValue == value) child.machine->Activate(world, context);
				}
			}

			void Exit(T value, World& world, const FrameContext& context)
			{
				for (const Child& child : children)
				{
					if (child.parentValue == value) child.machine->Deactivate(world, context);
				}
				RunHooks(HooksOf(exit, value), world, context);
				world.registry.template Each<DespawnOnExit<T>>([&](Entity entity, const DespawnOnExit<T>& scope)
				{
					if (scope.value == value) world.commands.Destroy(entity);
				});
				world.commands.Apply(world.registry);
			}

		public:
			StateMachine(T initialValue, State<T>& stateResource, NextState<T>& nextResource)
			    : initial(initialValue), state(&stateResource), next(&nextResource)
			{
			}

			void AddHook(bool onEnter, T value, std::string_view name, System system)
			{
				(onEnter ? enter : exit)[value].push_back({.system = system, .name = std::string(name)});
			}

			void AddChild(T parentValue, IStateMachine* machine) { children.push_back({parentValue, machine}); }

			void Process(World& world, const FrameContext& context) override
			{
				if (!state->active)
				{
					next->Clear(); // a sub-state whose parent is elsewhere ignores requests
					return;
				}
				if (!started)
				{
					started = true;
					Enter(state->value, world, context);
				}
				else if (next->requested)
				{
					const T target = *next->requested;
					next->Clear();
					if (target != state->value)
					{
						Exit(state->value, world, context);
						state->value = target;
						Enter(target, world, context);
					}
				}
				for (const Child& child : children)
				{
					child.machine->Process(world, context);
				}
			}

			void Activate(World& world, const FrameContext& context) override
			{
				state->value = initial;
				state->active = true;
				started = true;
				next->Clear();
				Enter(initial, world, context);
			}

			void Deactivate(World& world, const FrameContext& context) override
			{
				if (!state->active) return;
				Exit(state->value, world, context);
				state->active = false;
				next->Clear();
			}

			void BeginPass() noexcept override
			{
				for (auto* hooks : {&enter, &exit})
				{
					for (auto& entry : *hooks)
					{
						for (Hook& hook : entry.second)
						{
							hook.skipped = true;
						}
					}
				}
			}

			void AppendTimings(std::vector<Scheduler::SystemTiming>& out) const override
			{
				for (const auto* hooks : {&enter, &exit})
				{
					for (const auto& entry : *hooks)
					{
						for (const Hook& hook : entry.second)
						{
							out.push_back({.phase = Phase::Transition,
							               .name = hook.name,
							               .averageMs = hook.averageMs,
							               .skipped = hook.skipped});
						}
					}
				}
			}
		};

		template <StateValue T, typename Map> [[nodiscard]] StateMachine<T>& MachineOf(const Map& machineOf)
		{
			const auto it = machineOf.find(std::type_index(typeid(T)));
			ENGINE_VERIFY(it != machineOf.end(), "no state machine for this type: call AddState first");
			// Safe: the entry for typeid(T) is only ever created as a StateMachine<T>.
			return static_cast<StateMachine<T>&>(*it->second);
		}
	} // namespace detail

	template <StateValue T> void Scheduler::AddState(World& world, T initial)
	{
		ENGINE_ASSERT(!running, "cannot add states while a phase is running");
		ENGINE_VERIFY(!machineOf.contains(std::type_index(typeid(T))), "this state type already has a machine");
		auto& state = world.resources.Emplace<State<T>>(initial, true);
		auto& next = world.resources.Emplace<NextState<T>>();
		auto machine = std::make_unique<detail::StateMachine<T>>(initial, state, next);
		machineOf[std::type_index(typeid(T))] = machine.get();
		roots.push_back(machine.get());
		machines.push_back(std::move(machine));
	}

	template <StateValue Sub, StateValue Parent>
	void Scheduler::AddSubState(World& world, Parent parentValue, Sub initial)
	{
		ENGINE_ASSERT(!running, "cannot add states while a phase is running");
		ENGINE_VERIFY(!machineOf.contains(std::type_index(typeid(Sub))), "this state type already has a machine");
		detail::StateMachine<Parent>& parent = detail::MachineOf<Parent>(machineOf);
		auto& state = world.resources.Emplace<State<Sub>>(initial, false);
		auto& next = world.resources.Emplace<NextState<Sub>>();
		auto machine = std::make_unique<detail::StateMachine<Sub>>(initial, state, next);
		machineOf[std::type_index(typeid(Sub))] = machine.get();
		parent.AddChild(parentValue, machine.get());
		machines.push_back(std::move(machine));
	}

	template <StateValue T> void Scheduler::AddOnEnter(T value, std::string_view name, System system)
	{
		ENGINE_VERIFY(system != nullptr, "cannot add a null system");
		ENGINE_ASSERT(!running, "cannot add systems while a phase is running");
		detail::MachineOf<T>(machineOf).AddHook(true, value, name, system);
	}

	template <StateValue T> void Scheduler::AddOnExit(T value, std::string_view name, System system)
	{
		ENGINE_VERIFY(system != nullptr, "cannot add a null system");
		ENGINE_ASSERT(!running, "cannot add systems while a phase is running");
		detail::MachineOf<T>(machineOf).AddHook(false, value, name, system);
	}
} // namespace engine::ecs
