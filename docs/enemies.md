# Enemies

Four enemies roam the maze. They share how they move and when their mode changes, and differ in
where they aim. The numbers that are the same on every level live in
`game/include/enemytuning.hpp`; those that change per level are in `game/assets/levels.txt`
(see [difficulty.md](difficulty.md)). Names, looks, rules and numbers are all our own.

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

### Per-level numbers

The schedule's stretch lengths, how long a fright lasts, and the roaming and frightened speeds
change from level to level. They live in `game/assets/levels.txt`; see
[difficulty.md](difficulty.md) for the table and the format. Leaving the house (4.0 tiles per
second) and returning home (13.0) are the same on every level.

### Points

The first enemy eaten in one fright is worth 60, the second 120, the third 180, and so on.
Another power pellet restarts the count.

## Where the code is

| File                 | What                                                              |
|----------------------|-------------------------------------------------------------------|
| `enemytuning.hpp`    | The fixed tuning values, and the personality table.               |
| `difficulty.hpp`     | The per-level table read from `assets/levels.txt`.                |
| `steering.hpp`       | Exits, steering towards a target, random turns.                   |
| `enemymodes.hpp`     | The mode clock, the per-enemy state machine, speeds and goals.    |
| `targeting.hpp`      | Corners, the four chase rules, finding the house from its door.   |
| `enemysystems.hpp`   | The systems and event handlers that apply all of it to the world. |

Each frame, while playing: `EnemyModeSystem` steps the clock, `EnemySteeringSystem` picks each
enemy's next turn and speed, `GridMovementSystem` moves everyone, `CollisionSystem` publishes
`PlayerCaught` or `EnemyEaten`, and `EnemyLookSystem` updates colours and shapes.
