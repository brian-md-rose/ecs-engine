# 0002 – How systems announce what happened

**Status:** accepted · **Issue:** #24 · **Phase:** 5

## Context

From Phase 5 the game reacts to things that happen rather than to state: a pellet is eaten,
the player is caught, the last pellet goes. Several parts of the game care about each of
these (the score, later the sound and the HUD, the game's state machine), and the system that
notices it shouldn't have to know all of them. Calling each one directly would tie collision
to scoring to audio, and every new listener would mean editing the system that noticed.

The engine needs a way to publish a typed event once and have it reach whoever is interested,
without knowing the game's event types, and it has to fit the scheduler: systems are plain
functions run in phases, and structural changes are deferred to the end of each phase
(0001, `CommandBuffer`).

## Options

1. **Immediate dispatch.** `Publish` calls every handler on the spot. Simple, but handlers run
   in the middle of whatever system published, often inside a `Registry::Each` loop where
   structural changes assert, and a handler sees a half-updated world (the pellet that was
   just eaten still exists because its `Destroy` is only queued).
2. **Queues that systems read.** Each event type has a queue kept as a resource; systems that
   care read it later in the frame, and the queue is cleared at a fixed point. Explicit and
   ECS-like, but every reader becomes a system that must be registered after every writer,
   and "who reads this" is spread across the schedule. Clearing needs care so a reader in an
   earlier phase doesn't miss events from a later one.
3. **Deferred publish/subscribe.** Handlers subscribe to a type during setup; `Publish` only
   queues; the queue is delivered at a defined point, outside any system.

## Decision

Option 3. `engine::ecs::EventBus` (`engine/include/ecs/events.hpp`) lives on the `World` as
`world.events`, next to the registry, resources and command buffer: it is engine machinery
every game gets, not a game resource.

```cpp
// Setup (MazeGame's constructor):
world.events.Subscribe<PelletEaten>(AddPelletScore);
world.events.Subscribe<PelletEaten>(CheckLevelCleared);

// In a system:
world.commands.Destroy(pellet);
world.events.Publish(PelletEaten{.tile = tile});
```

### When events are delivered

`Scheduler::Run` ends every phase the same way:

1. Run the phase's systems.
2. Apply `world.commands`.
3. `world.events.Deliver(world)`.
4. Apply `world.commands` again, for commands the handlers queued.

So handlers run after the world has settled (the eaten pellet is gone, so counting what is
left is correct), outside any `Each` loop, and before the next phase starts. The queue is
empty at the end of every phase, which is the one point where events are cleared: an event
never outlives the phase it was published in.

### Order

- Events are delivered in the order they were published, across all types.
- Each event goes to its type's handlers in the order they subscribed, so setup decides,
  for example, that the score is added before the HUD prints it.
- Events published by a handler are queued behind the rest and delivered in the same
  `Deliver` call (breadth-first). A cycle would never end, so `Deliver` aborts past
  `kMaxEventsPerDelivery` events.

## Rules

1. **Handlers are plain functions**, `void(World&, const T&)`, like systems: state they keep
   goes in a resource.
2. **Subscribe during setup.** Subscribing while events are being delivered asserts.
3. **Handlers follow the system rules**: structural changes go through `world.commands`, and
   resources can't be added or removed (they are locked during delivery as during the phase).
4. **Events are small values**, copied into the queue: a tile, an entity, an amount. Not
   references into the world, which may have changed by the time they are delivered.
5. **Events nobody subscribes to are dropped** at `Publish`, so announcing something no one
   listens to yet (audio, before Phase 7) costs nothing.

## Consequences

- Systems that notice things don't know who listens, and new listeners (audio, the HUD) are a
  `Subscribe` line in setup.
- A reaction happens at the end of the phase, not at once. Within a phase a system can't see
  the effects of an event published earlier in that phase; it sees them in the next phase.
  So far that is what we want: the game's collisions run in LateUpdate and their effects
  (score, level cleared) are settled before the next update's Input.
- Control flow is less visible than a direct call: what happens on `PelletEaten` is found by
  searching for `Subscribe<PelletEaten>`. Keeping every subscription in the game's setup, in
  order, next to the system registrations, is the mitigation.
