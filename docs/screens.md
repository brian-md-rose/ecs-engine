# Screens

How Tangle Run moves between its screens. The mechanism is the engine's state system
([ADR 0003](decisions/0003-game-states.md)); the states are in `game/include/gameflow.hpp` and
what each one sets up is registered in `game/src/gamestates.cpp`.

```
Title ──START──► Playing ──(no lives, new high score)──► NameEntry ──► GameOver ──► Title
  │                │  │
  │                │  └──(no lives)────────────────────────────────────► GameOver
  │                ├──(level cleared)──► LevelTransition ──(2.5 s)──► Playing
  │                └──(quit from pause, new high score)──► NameEntry
  └──OPTIONS──► Options ──Esc / BACK──► Title
                   └──UI DEMO──► UiDemo ──Esc / BACK──► Options
```

Inside `Playing`, `Round` goes Ready (2 s) → Active → Dying (1.5 s) → Ready, and `Pause` goes
Running ⇄ Paused. Quitting from the pause menu goes to the title, by way of the name entry
when the score beats the record.

## The world

The title and options screens show only their own menus on black. The maze, pellets, player,
enemies and HUD are created by `SpawnGameWorld` when `Playing` is entered (not by the START
button, so anything that goes straight to `Playing` gets a world too). The `GameWorld` resource
records that they exist, so coming back to `Playing` from a `LevelTransition` does not spawn a
second copy; the pellets are reset when the level screen ends. Every one of those entities
carries `DespawnOnEnter<Screen>{Title}`, so entering the title removes them, and `EnterTitle`
clears the flag. NameEntry and GameOver keep the world, dimmed, behind them. The maze file and
assets are still loaded and checked at launch, so a broken maze is reported before the title.

## Options

Up and down choose, left and right (or Enter) change, Esc or BACK goes back.

| Item  | Values             | Effect                                          |
|-------|--------------------|-------------------------------------------------|
| SOUND | ON, OFF            | OFF mutes every sound effect (`AudioSystem`)    |
| LIVES | 1 to 9 (default 3) | Tries at the start of each new game             |
| DISPLAY | WINDOWED, BORDERLESS, FULLSCREEN | Applies at once (see [display.md](display.md)) |
| SCALING | INTEGER, FIT | Whole-number scale with bars, or any scale letterboxed |
| UI DEMO | (opens a screen) | The mouse UI demo, see below                  |

Saved to `save.txt` when the screen is left (and when the game closes) as `sound 0|1` and
`lives N`, `display 0|1|2` and `scaling 0|1`, and loaded at start-up. A missing or out-of-range value keeps its default.

## UI demo

OPTIONS, then UI DEMO. A panel with a click counter, three buttons (COUNT adds one, RESET sets 0, BACK returns
to Options) and a list of 30 rows, some wrapped onto several lines, that scrolls with the wheel or by dragging.
Esc also goes back. The pointer is drawn as a sprite (the system pointer is hidden by `MazeGame::ShowSystemCursor`).
It is built entirely from the engine's `ui/` module, see [ui.md](ui.md); the code is `game/src/uidemo.cpp`.
Nothing of it exists outside the screen: the UI systems run only in `UiDemo`, and everything it spawns goes when it
ends.

## Name entry

After a game that beats the high score. The eight slots start from the last name used
(`A` for a first-time player). Up and down change the letter in the current slot through
A to Z, 0 to 9 and a space; left and right move between slots; Enter records the score under
the name and goes on to GAME OVER. A blank name is saved as `PLAYER`.

Saved as `highScore N` and `highScoreName NAME`. The title screen shows both under the menu.
Closing the window during a game saves a score that beats the record under the last name used.

## Level transition

Clearing the last pellet shows WELL DONE!, `LEVEL n` and the score for 2.5 seconds over the
cleared maze, then the next level starts with its pellets back and the score carried on.

## save.txt

```
highScore 1895
highScoreName ADA
lives 3
sound 1
```

Other lines are left alone when the game saves.
