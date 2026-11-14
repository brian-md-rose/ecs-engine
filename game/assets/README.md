# Game assets

Everything the game loads at run time. The build copies this folder next to the game
executable (the `game_assets` target in `game/CMakeLists.txt`), and the game looks for it there
with `engine::core::ExecutableDirectory()`, so it runs the same from Visual Studio, the build
folder or anywhere else.

The copy adds and overwrites files but never deletes them: after removing or renaming an asset,
delete the stale copy from the build folder (or rebuild from clean).
