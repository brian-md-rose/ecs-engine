# 0003 – Game states

**Status:** accepted · **Phase:** 8

## Context

The game's screens were a `Stage` enum in a `GameFlow` resource plus a `paused` flag. Every
system that cared checked them itself (`IsPlaying`, `NotPaused`). The title and pause menus
were spawned once and shown or hidden every update. A change of stage was an event
(`StageChanged`) whose handlers each decided, from the old and new stage, whether to reset
something (`ResetFor(from, to)`). Adding a screen meant touching all of that, and a screen's
set-up and tear-down lived in several places.

## Decision

The engine gets states, modelled on Bevy's (`engine/include/ecs/states.hpp`).

- `State<T>` is the current value of an enum `T`, a resource. `NextState<T>` holds a requested
  change. Systems read `State` and write `NextState`; they never change `State` themselves.
- **When a change happens.** `Scheduler::RunTransitions` applies requests once per update,
  before Input (`ScheduledGame::Update` calls it first, even before interpolation records
  positions, so a reset isn't smeared across frames). A change asked for during any phase, or
  by an enter or exit system, takes effect at the start of the next update. Within one update
  every system sees one state. Asking for the current value does nothing: no exit, no enter.
- **Enter and exit systems.** `AddOnEnter(value, name, system)` and `AddOnExit(...)`. On a
  change the old value's exit systems run, then the new value's enter systems, each with the
  commands it queued applied before the next runs. The first update runs the initial value's
  enter systems.
- **Run conditions.** `InState<Screen::Playing>` is a `RunCondition` (the value is a template
  argument because conditions are plain function pointers); it takes several values of one
  type. `NotInState` is its opposite.
- **Scoped entities.** `DespawnOnExit<T>{value}` destroys the entity when `T` leaves `value`.
  Screens spawn their text on enter and are gone on exit, with no one hiding anything.
  `DespawnOnEnter<T>{value}` is the other way round: it destroys the entity when `T` enters
  `value`, **before** the enter systems run, so what an enter system spawns with the same scope
  survives the entry that spawned it. A game uses it to clear its world on return to a menu.
- **Sub-states.** `AddSubState<Sub>(world, parentValue, initial)`. A sub-state exists only while
  its parent is in `parentValue`: it starts from `initial` on each entry, its exit systems run
  before the parent's when the parent leaves, and while it doesn't exist `InState` is false for
  all its values and requests for it are dropped.
- **Timings.** Enter and exit systems have names and show in `Scheduler::Timings()` (phase
  `Transition`), so the F3 panel lists them. They show "OFF" when they didn't run in the last
  pass, which is most of the time.

Unlike Bevy, there is one fixed schedule, so there are no `OnTransition(from, to)` systems and
no "re-enter the same state" request; a state that needs to restart goes through another value.

## The game's states (`game/include/gameflow.hpp`)

- `Screen`: Title, Options, NameEntry, Playing, LevelTransition, GameOver.
- `Round` (sub-state of Playing): Ready, Active, Dying.
- `Pause` (sub-state of Playing): Running, Paused.

Round and Pause are separate sub-states because pausing must remember which round it
interrupted, and a sub-state of a sub-state would start over on resume. The cost is that
"the round is live" needs two conditions (`IsRoundLive`).

`GameFlow` is now only data: lives, level and the clock of the current timed state.
`StageChanged`, `ResetFor` and the `Stage` state machine are gone. Events remain for things that
happened in the world (`PelletEaten`, `PlayerCaught`, `LevelCleared`).

## Consequences

- A change of screen has a one-update latency (16 ms). Tests tap a key and update once more.
- Two events that both want to end the round in one update (caught on the last pellet) are
  settled by the handler: the first request wins, the later handler sees a change is pending.
- Adding a screen is: an enum value, a spawn function, an input system with an `InState`
  condition, and its enter system registered in `gamestates.cpp`.
