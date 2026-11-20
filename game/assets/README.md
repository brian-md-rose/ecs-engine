# Game assets

Everything the game loads at run time. The build copies this folder next to the game
executable (the `game_assets` target in `game/CMakeLists.txt`), and the game looks for it there
with `engine::core::ExecutableDirectory()`, so it runs the same from Visual Studio, the build
folder or anywhere else.

The copy adds and overwrites files but never deletes them: after removing or renaming an asset,
delete the stale copy from the build folder (or rebuild from clean).

## music/

`theme.ogg` is the looping music (Ogg Vorbis, played by `MusicSystem` on the music bus, `kMusicFile` in `sounds.hpp`). It is
an original chiptune loop of four chords, eight bars. Without the file the game just runs without music.

## scenes/

JSON scene files that lay out the screens: `title`, `help`, `options`, `nameentry`, `leveltransition`, `pause` and `uidemo`.
The game loads every `.json` here at start-up and a broken file stops it. `SpawnScene` (`game/src/scenes.cpp`) puts a screen's
file into the world when the screen opens. Open and edit them in `tangle_run_editor`. See [docs/screens.md](../../docs/screens.md)
and [docs/scenes.md](../../docs/scenes.md).

## Other files

`levels.txt` is the difficulty table ([difficulty.md](../../docs/difficulty.md)), `mazes/` holds the maze files
([maze-format.md](../../docs/maze-format.md)) and `sprites/` the sprite and font sheets ([pixel-sheets.md](../../docs/pixel-sheets.md)).
