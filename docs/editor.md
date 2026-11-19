# The editor overlay

`engine::editor` is a [Dear ImGui](https://github.com/ocornut/imgui) overlay drawn on top of a running game. This is its first step: the hook and an empty shell. The hierarchy and inspector panels come next (plan step E4). It is off by default, so a game that doesn't want it never compiles it and `engine::platform` doesn't depend on it.

## Building it

```
git submodule update --init thirdparty/imgui
cmake -S . -B build/editor -DENGINE_BUILD_EDITOR=ON
```

That adds `engine::editor`, the `tangle_run_editor` executable (Tangle Run with the overlay) and, with tests on, `editor_tests`. CI builds it on Windows (MSVC); the code is plain SDL3 and ImGui and also builds with GCC and Clang.

## Using it

In `tangle_run_editor`, F1 (which the game never sees) shows and hides the overlay. While it is shown, a menu bar appears with **Panels** (the ImGui demo window for now) and **Help > About**. The mouse and keyboard go to the overlay's windows when the pointer is over one or a text box has focus, and to the game otherwise. The system mouse pointer is shown while the overlay is, even when the game draws its own.

To add it to another game:

```cpp
engine::editor::Editor editor;
editor.AddPanel("My panel", [] { ImGui::Text("hello"); });
app.SetEditorHost(&editor);
app.Run(game);
```

and link `engine::editor`. A panel is any function that calls ImGui; it gets its own window, opened from the Panels menu.

## How it hooks in

`application/editorhost.hpp` defines `EditorHost`, with no SDL types (the window, renderer and events cross as opaque pointers). `Application::SetEditorHost` takes one, not owned, and then:

- calls `Attach` just before the main loop and `Detach` after it;
- passes every SDL event to `OnEvent` first, and skips the game's own handling when it returns true (a close request is never taken);
- calls `Draw` after the game's `Render` and before the frame is presented;
- asks `WantsSystemCursor` each frame.

With no host set, none of this costs anything. The overlay draws at real window pixels, not the game's logical size, so its text stays sharp: `Editor::Draw` turns the renderer's logical presentation off for the overlay's draw calls and back on after. It writes no `imgui.ini`.

ImGui is a submodule pinned to a release tag (v1.92.9b); to update it, check out another tag in `thirdparty/imgui` and commit.
