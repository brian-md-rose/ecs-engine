# Difficulty curve

How hard each level is lives in `game/assets/levels.txt`, so the curve can be tuned without
rebuilding: edit the file and run the game (the build copies it next to the executable). The
game reads it at start-up (`LoadDifficulty`, `game/include/difficulty.hpp`) and refuses to
start if it is malformed, printing the file, line and column of the problem.

## Format

```text
# A comment. Blank lines are ignored.
#      level  player  enemy  frightened  fright  patrol  chase  patrol  chase  patrol
level  1      7.50    6.75   4.25        7.0     6       18     6       24     4
```

- One `level` line per level, numbered 1, 2, 3, ... in order. Levels past the last line play
  like the last.
- `player`, `enemy`, `frightened`: speeds in tiles per second. `enemy` is a roaming enemy's
  speed while it patrols or chases; `frightened` is its speed while frightened.
- `fright`: how long a power pellet frightens the enemies, in seconds. They flash for the last
  2 seconds of it (`kFrightWarningSeconds`).
- `patrol chase patrol chase patrol`: the lengths of the first five stretches of the enemies'
  schedule, in seconds. After them the enemies chase until the round ends.
- Every number must be above 0; speeds at most 30 and times at most 600 (typo guards).

Speeds that don't change with the level stay in `game/include/enemytuning.hpp`: leaving the
house (4.0) and hurrying home after being eaten (13.0).

## The curve

| Level | Player | Enemies | Frightened | Fright (s) | Patrol / chase (s)     |
|------:|-------:|--------:|-----------:|-----------:|------------------------|
| 1     | 7.50   | 6.75    | 4.25       | 7.0        | 6 / 18 / 6 / 24 / 4    |
| 2     | 7.60   | 7.00    | 4.40       | 6.0        | 5 / 22 / 4 / 30 / 3    |
| 3     | 7.75   | 7.25    | 4.50       | 5.0        | 5 / 24 / 4 / 32 / 3    |
| 4     | 7.90   | 7.50    | 4.60       | 4.0        | 4 / 26 / 3 / 40 / 2    |
| 5     | 8.00   | 7.75    | 4.75       | 3.5        | 4 / 30 / 3 / 45 / 2    |
| 6     | 8.10   | 8.00    | 4.90       | 3.0        | 3 / 34 / 2 / 50 / 1    |
| 7     | 8.25   | 8.30    | 5.00       | 2.5        | 3 / 40 / 2 / 60 / 1    |
| 8+    | 8.25   | 8.50    | 5.00       | 2.5        | 2 / 45 / 1 / 75 / 1    |

Each level squeezes the player from three sides at once:

- **The enemies close the speed gap.** On level 1 the player is 0.75 tiles per second faster
  than the enemies; from level 7 the enemies are faster, so the player can no longer simply
  outrun them in a straight corridor.
- **Power pellets buy less time.** Frights shrink from 7 seconds to 2.5, so from level 7 the
  enemies flash almost as soon as they turn, and frightened enemies get quicker too.
- **Fewer breathers.** Patrols (when the enemies wander off to their corners) shrink from 6
  seconds to 1-2, and chases grow, so the enemies spend almost all their time hunting.

All numbers are our own. Games built without the file (the tests) use `FallbackDifficulty()`,
the Phase 6 tuning for levels 1-5.
