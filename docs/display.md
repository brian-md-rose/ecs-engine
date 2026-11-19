# Display settings

How the game is shown: window mode and size, VSync, a frame cap, and how the logical picture is scaled to the window.
The types are in `graphics/display.hpp` and have no SDL in them. The game never calls into the window or renderer; it asks
through a resource, and `Application` does the work after the frame.

## Asking for a change

```cpp
world.resources.Emplace<graphics::DisplayRequest>();   // at setup
world.resources.Emplace<graphics::DisplayInfo>();

// in a system, when the player picks something:
world.resources.Get<graphics::DisplayRequest>().pending =
    graphics::DisplaySettings{.mode = WindowMode::Fullscreen, .width = 1920, .height = 1080,
                              .vsync = true, .scaling = Scaling::Fit};
```

`ScheduledGame` hands the two resources to `Application` through `Game::Display()`. After each frame `Application`
applies `pending`, clears it, and refreshes `DisplayInfo`: `current` is what is in force and `resolutions` is the list of
sizes the display can switch to, largest first, each once (empty without a display). A game that does not override
`Display()`, or a headless run, simply ignores requests. A request is checked first (`Sanitized`): sizes are kept between
`kMinWindowSize` and `kMaxWindowSize`, and the frame cap between 0 and `kMaxFrameCap`.

If SDL refuses part of a request (a mode the display cannot do), the reason is logged, that part is left as it was, and
`DisplayInfo::current` says what is really in force. Compare it with what you asked for if you care.

## What each setting does

| Setting | Meaning |
|---------|---------|
| `mode` | `Windowed`: a framed, resizable window of `width` x `height`. `Borderless`: fills the desktop, no frame, no display change (width and height are ignored). `Fullscreen`: exclusive; the display switches to the closest mode to `width` x `height` |
| `vsync` | tie frames to the display's refresh |
| `frameCap` | with `vsync` off, frames per second to stop at (0 for none); the loop sleeps out the rest of each frame |
| `scaling` | `Integer`: the largest whole-number scale that fits, so pixels stay square, with bars around the rest. `Fit`: any scale, letterboxed, so a 960 x 540 picture fills a 1280 x 800 window to 1280 x 720 |

`Presentation(logicalW, logicalH, windowW, windowH, scaling)` gives the rectangle the picture lands in, for tests or
tools. When the player resizes a framed window by hand, `current.width` and `height` follow.

The starting values come from `ApplicationConfig` (`scaling`, `vsync`, `frameCap`; the window starts framed at
`windowWidth` x `windowHeight`). `Renderer` also takes the scaling mode in its constructor and has `SetScaling` and
`SetVSync`; `Window` has `SetMode` and `Resolutions`.

## Update limit

`FixedTimestep` cuts a long stall to 0.25 s of real time, but at a time scale above 1 that is still more updates (30 at x2, 60
at x4). If those take longer than the frame they replace, the next frame is behind again, and the game spirals. So
`FixedTimestep::SetMaxUpdates(n)` caps the updates one frame runs and drops the backlog beyond it, and `Application`
sets it from `ApplicationConfig::maxUpdatesPerFrame` (15 by default, which is what a stalled frame gets at normal speed, so
nothing changes unless the game runs faster than real time). 0 means no limit.

## In Tangle Run

OPTIONS has DISPLAY (windowed, borderless, fullscreen) and SCALING (integer, fit), saved in `save.txt` as `display` and
`scaling`. `DisplaySystem` turns a change, or the saved choice at start-up, into a `DisplayRequest`. Fullscreen asks for the
display's biggest size, and going back to windowed restores the window's earlier size. A choice the display refuses is
not asked for again every update.
