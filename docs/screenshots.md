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

1. In a development build, the repository's `screenshot/` folder (`ENGINE_SCREENSHOT_DIR` in CMake, which defaults to
   `<repo>/screenshot`), as long as that folder exists. Its PNGs are git-ignored; `.gitkeep` keeps the folder.
2. Otherwise `screenshots/` next to `log.txt` in the user's save folder, for example
   `%APPDATA%\ecs-engine\Tangle Run\screenshots\`. Build a release you ship with `-DENGINE_SCREENSHOT_DIR=` (empty) so
   it always uses this one.

With no folder set, a screenshot fails with a logged reason and nothing is written.

## From code

- `Application::SetScreenshotKey(std::optional<input::Key>)` changes the key, or turns it off with `std::nullopt`.
- `Application::RequestScreenshot()` takes one of the next frame, as the key does.
- A game can override `Game::Screenshot()` to return a `ScreenshotLink` (`graphics/screenshot.hpp`): set
  `ScreenshotRequest::pending` to ask, and read `ScreenshotResult` for the last file, the last error and a count.
- `Renderer::ReadWindowPixels()` and `ReadLogicalPixels()` do the readback, between drawing and `Present`.
- `ResizeImage`, `CropImage`, `ScreenshotFileName` and `UniqueScreenshotPath` need no window and are in `engine::core`.
