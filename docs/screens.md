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
  ├──HOW TO PLAY──► Help ──Esc / BACK──► Title
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

## Layouts are scenes

Every screen except the playing field is laid out by a scene file in `game/assets/scenes/`: `title`, `help`, `options`,
`nameentry`, `leveltransition`, `pause` and `uidemo`. The screen's enter system calls `SpawnScene` (`scenes.hpp`),
which loads the file, resolves its `ClipName`s (a scene names a picture, the atlas rectangle is filled in afterwards)
and scopes what it made to the state, so leaving the state removes it. What a file can't hold stays in code:

- the changing text, as a `ScreenText` component with a `kind`: menu lines with their `> ` cursor, the options from the
  settings, the name slots, `SCORE n` and `LEVEL n` (a `label` followed by the number), and the title's high score,
  which is blank until there is a record. `ScreenTextSystem` writes them every frame;
- the how-to-play rows (where each goes depends on how tall the ones before it wrap), spawned under the list's content
  entity, which also sets the scroll view's content height.

Centred lines are `Align::Centre` text at x 112 (half the 224-wide screen), so editing the words keeps them centred.
The game loads every `.json` in `assets/scenes` at start-up, and a broken file stops it like any other asset; if a
scene is missing or fails to load when its screen opens, the screen is empty and the errors are logged. Open the
files in `tangle_run_editor` to move things about ([editor.md](editor.md)).

## The menus

The title, options and pause menus and the how-to-play screen are made of the engine's UI pieces
([ui.md](ui.md)): a nine-patch panel, a nine-patch button for each item (`MenuButton`, with a `Hitbox`,
`Interactable` and `FocusOrder`) and its label as a child, so it moves with the button. `game/include/menuui.hpp` and
`game/src/menuui.cpp` hold the glue.

- **Pointer:** moving onto a button selects it, and clicking acts on it exactly as Enter would (on the options, a click
  steps the setting forward). `UiPointerSystem` runs on every menu screen and on the pause menu.
- **Keys and pad:** up, down and confirm move the same `MenuCursor`. `Controls` already turns the d-pad, the left stick
  and the south button into those actions, so the menus don't also run the engine's focus-moving system (that would
  step twice). The selected button holds the engine's `Focused` tag and looks hovered; moving the pointer onto another
  button hands the selection over, and the keys carry on from there.
- A click is published by the pointer system and delivered at the end of the Input phase, so it is acted on in the next
  frame's menu system (`TakeMenuClick`), which takes it and ignores the confirm key that frame.

## How to play

TITLE, then HOW TO PLAY: the controls and rules in a scroll view (wheel, drag, or the right stick, since the view holds
`Focused`) and a BACK button. Esc, Start, Enter or a click on BACK return to the title. The text is `HelpLines()` in
`game/src/screens.cpp`.

## Options

Up and down choose, left and right (or Enter, or a click) change, Esc or BACK goes back.

| Item  | Values             | Effect                                          |
|-------|--------------------|-------------------------------------------------|
| SOUND | ON, OFF            | OFF mutes every sound effect (`AudioSystem`)    |
| MUSIC | ON, OFF            | OFF fades the music out (`MusicSystem`)         |
| VOLUME | 0 to 10 (default 8) | The master volume; 0 also stops the music    |
| LIVES | 1 to 9 (default 3) | Tries at the start of each new game             |
| DISPLAY | WINDOWED, BORDERLESS, FULLSCREEN | Applies at once (see [display.md](display.md)) |
| SCALING | INTEGER, FIT | Whole-number scale with bars, or any scale letterboxed |
| UI DEMO | (opens a screen) | The mouse UI demo, see below                  |

Saved to `save.txt` when the screen is left (and when the game closes) as `sound 0|1`, `music 0|1`, `volume N` and
`lives N`, `display 0|1|2` and `scaling 0|1`, and loaded at start-up. A missing or out-of-range value keeps its default.

## UI demo

OPTIONS, then UI DEMO. A panel with a click counter, three buttons (COUNT adds one, RESET sets 0, BACK returns
to Options) and a list of 30 rows, some wrapped onto several lines, that scrolls with the wheel or by dragging.
Esc also goes back. The pointer is drawn as a sprite (the system pointer is hidden by `MazeGame::ShowSystemCursor`).
It is built entirely from the engine's `ui/` module, see [ui.md](ui.md); the code is `game/src/uidemo.cpp`.
Nothing of it exists outside the screen: its buttons and list are removed when it ends. It is the one screen that uses
the engine's focus-moving system (`UiFocusSystem`, for Tab and the pad); the real menus use the cursor, see above.

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
