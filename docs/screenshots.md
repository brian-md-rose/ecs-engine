# Screenshots

Press **F12** in any game built on the engine to save a PNG of the current frame. Nothing else is needed in the game.

## What is saved

- In a game: the game's picture at its **logical** size (320x180 for Tangle Run), without the letterbox bars, whatever
  the window size and scaling. With Integer scaling this is pixel-exact; with Fit scaling the picture is averaged down.
- With the editor overlay (`EditorHost`): the **whole window**, ImGui panels included, at the window's size.

The file is named `screenshot-YYYYMMDD-HHMMSS.png` in local time. Two in one second get `-2`, `-3` and so on, so nothing
is overwritten. The path is logged (`screenshot saved: ...`).

## Where it goes

`graphics::ScreenshotDirectory(saveDirectory)` decides, and the game passes it to `Application::SetScreenshotFolder`:

1. In a Debug (or RelWithDebInfo) build, the repository's `screenshot/` folder (`ENGINE_SCREENSHOT_DIR` in CMake, which
   defaults to `<repo>/screenshot`), as long as that folder exists. Its PNGs are git-ignored; `.gitkeep` keeps the folder.
2. Otherwise `screenshots/` next to `log.txt` in the user's save folder, for example
   `%APPDATA%\ecs-engine\Tangle Run\screenshots\`. **Release and MinSizeRel builds always use this one**: the repository
   path is only compiled in for the other configurations. `-DENGINE_SCREENSHOT_DIR=` (empty) turns it off everywhere.

With no folder set, a screenshot fails with a logged reason and nothing is written.

## From code

- `Application::SetScreenshotKey(std::optional<input::Key>)` changes the key, or turns it off with `std::nullopt`.
- `Application::RequestScreenshot()` takes one of the next frame, as the key does.
- A game can override `Game::Screenshot()` to return a `ScreenshotLink` (`graphics/screenshot.hpp`): set
  `ScreenshotRequest::pending` to ask, and read `ScreenshotResult` for the last file, the last error and a count.
- `Renderer::ReadWindowPixels()` and `ReadLogicalPixels()` do the readback, between drawing and `Present`.
- `ResizeImage`, `CropImage`, `ScreenshotFileName` and `UniqueScreenshotPath` need no window and are in `engine::core`.

## From a script

`game.exe` and `tangle_run_editor.exe` take two options, so a script, CI or a Claude session can get a picture without
pressing a key:

```
game.exe --screenshot-after 120 --screenshot-dir C:\temp\shots
```

- `--screenshot-after N` saves frame N (counting from 1) and then closes the program. The exit code is 1 if the file could
  not be saved, 0 otherwise. In the editor it is the whole editor window.
- `--screenshot-dir PATH` saves there instead of the usual folder (made if missing). It works in Release builds too.
- Both also accept `--name=value`. A bad value is logged and ignored.

Programs with their own arguments call `graphics::ParseScreenshotArgs` and `Application::SetScreenshotArgs`, and skip
the options' entries with `graphics::ScreenshotArgWidth`.

The editor needs no extra screenshot code for Play: F12 saves the whole window, including the Viewport that shows the
running game.
