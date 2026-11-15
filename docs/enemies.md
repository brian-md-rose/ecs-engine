# Enemies

Four enemies roam the maze. They share how they move and when their mode changes, and differ in
where they aim. Every number below lives in `game/include/enemytuning.hpp`; change it there.
Names, looks, rules and numbers are all our own.

## Moving: target-tile steering

Each enemy has a **target tile**. Each time it comes to a tile it chooses which way to leave
it (`steering.hpp`):

1. It may take any open side (walls never; the house door only while going in or out) **except
   the way it came**. It turns back only at a dead end, or when its mode changes (below).
2. Of those, it takes the one whose next tile is **closest to the target in a straight line**.
3. A tie goes to the side that comes first in the order **right, down, left, up**.

The target may be off the maze; the enemy still heads that way. An enemy decides a tile's turn
on its way to the tile's centre and takes it there, using the same grid movement as the player.

## The four personalities

Spawns (`E` in the maze) get the personalities in reading order: the first `E` is Grit, the
second Snare, and so on.

| Enemy | Look                    | Leaves the house | Patrols to   | Chase rule |
|-------|-------------------------|------------------|--------------|------------|
| Grit  | green square            | at once          | top-left     | **The bloodhound.** Aims at the player's tile. |
| Snare | violet circle           | after 2 s        | top-right    | **The trapper.** Aims at the next junction on the player's way (a tile with 3+ open sides), looking at most 6 tiles ahead; the player's tile if they stand still. |
| Clasp | small white square      | after 5 s        | bottom-left  | **The flanker.** Aims 5 tiles past the player on the side away from Grit, along the axis Grit is further off on (across, on a tie). |
| Wisp  | small sand-coloured circle | after 8 s     | bottom-right | **The stalker.** Chases while more than 6 tiles away (across plus down); at 6 or closer it aims as far again beyond itself, directly away from the player. |

"Patrols to" is the maze's corner tile on that side, usually a wall, so a patrolling enemy loops
around the walls nearest its corner.

## Modes

All enemies share one clock (`ModeClock`, `enemymodes.hpp`). Each enemy also has a state:

```text
InHouse ──(its release time)──► Leaving ──(out of the door)──► Roaming
                                   ▲                               │ eaten while frightened
                                   └──────(reached home)── Returning ◄┘
```

- **Roaming** enemies **patrol** (head for their corner) or **chase** (their chase rule), all
  together, following the level's schedule.
- **Frightened**: a power pellet frightens every enemy not on its way home. A frightened enemy
  turns at random at each tile, moves slower, can be eaten, and flashes for the last 2 s of the
  fright. While a fright lasts the schedule waits.
- **Returning**: an eaten enemy shrinks to a dot and hurries back through the door to the tile
  just inside it, then turns round and leaves again, no longer frightened.
- **Turning back**: a roaming enemy turns back at once when the schedule switches between
  patrol and chase, and when it becomes frightened. The end of a fright does not turn it back.

Being caught by an enemy that is neither frightened nor returning costs a life (the game goes
to Dying). When the next round starts, the enemies go back into the house and the clock starts
over. A maze without a door has no house: its enemies start out in the maze, and an eaten one
goes back to its spawn.

### Schedule (seconds)

The last stretch lasts until the round ends.

| Levels | Patrol | Chase | Patrol | Chase | Patrol | Chase |
|--------|-------:|------:|-------:|------:|-------:|------:|
| 1      | 6      | 18    | 6      | 24    | 4      | for good |
| 2–3    | 5      | 22    | 4      | 30    | 3      | for good |
| 4+     | 4      | 26    | 3      | 40    | 2      | for good |

### Fright length

| Level   | 1 | 2 | 3 | 4 | 5+ |
|---------|---|---|---|---|----|
| Seconds | 7 | 6 | 5 | 4 | 3  |

### Speeds (tiles per second)

| Player | Roaming | Frightened | Leaving the house | Returning |
|-------:|--------:|-----------:|------------------:|----------:|
| 7.5    | 6.75    | 4.25       | 4.0               | 13.0      |

### Points

The first enemy eaten in one fright is worth 60, the second 120, the third 180, and so on.
Another power pellet restarts the count.

## Where the code is

| File                 | What                                                              |
|----------------------|-------------------------------------------------------------------|
| `enemytuning.hpp`    | Every tuning value above, and the personality table.              |
| `steering.hpp`       | Exits, steering towards a target, random turns.                   |
| `enemymodes.hpp`     | The mode clock, the per-enemy state machine, speeds and goals.    |
| `targeting.hpp`      | Corners, the four chase rules, finding the house from its door.   |
| `enemysystems.hpp`   | The systems and event handlers that apply all of it to the world. |

Each frame, while playing: `EnemyModeSystem` steps the clock, `EnemySteeringSystem` picks each
enemy's next turn and speed, `GridMovementSystem` moves everyone, `CollisionSystem` publishes
`PlayerCaught` or `EnemyEaten`, and `EnemyLookSystem` updates colours and shapes.
