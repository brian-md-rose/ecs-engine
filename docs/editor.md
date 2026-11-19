# The editor overlay

`engine::editor` is a [Dear ImGui](https://github.com/ocornut/imgui) overlay drawn on top of a running game. It has the hook and shell (E3) and the Hierarchy and Inspector panels (E4). Scene files, Play and Stop, the viewport and undo come later, and are parked for now. It is off by default, so a game that doesn't want it never compiles it and `engine::platform` doesn't depend on it.

## Building it

```
git submodule update --init thirdparty/imgui
cmake -S . -B build/editor -DENGINE_BUILD_EDITOR=ON
```

That adds `engine::editor`, the `tangle_run_editor` executable (Tangle Run with the overlay) and, with tests on, `editor_tests`. CI builds it on Windows (MSVC) only. The code is plain SDL3 and ImGui, and was also built with GCC and Clang during development, but nothing checks that.

## Using it

In `tangle_run_editor`, F1 (which the game never sees) shows and hides the overlay. While it is shown, a menu bar appears with **Panels** (Hierarchy, Inspector and the ImGui demo window) and **Help > About**. The mouse and keyboard go to the overlay's windows when the pointer is over one or a text box has focus, and to the game otherwise. The system mouse pointer is shown while the overlay is, even when the game draws its own.

## Hierarchy and Inspector

`AddEntityPanels(editor, world)` (`editor/entitypanels.hpp`) adds both panels for a `ecs::World`. The world needs a `TypeRegistry` resource with the components registered (see [scenes](scenes.md)); only registered components are shown.

- **Hierarchy:** every entity as a tree by `Parent`, labelled by its `Name` (or `Entity <index>`), with a filter box. Click selects. Drag an entity onto another to make it a child, or onto the "top level" line to unparent it; a move that would make an entity its own ancestor is refused. Right-click for *Add child*, *Duplicate* (the entity and everything under it, with references between them pointing at the copies) and *Delete* (with everything under it). *New entity* makes one at the top level.
- **Inspector:** the selected entity's registered components, each in a collapsible section (the close button removes the component), and *Add component* for the ones it lacks. Fields are widgets by their registered kind: checkbox, drag or slider (using the field's min and max hints), text, combo for enums, an entity picker for entity references and lists, and a generic number editor for custom types such as a `Color`. Components marked Runtime are hidden. Read-only fields are greyed out and a field's tooltip shows on hover.

The panels change the registry directly, between frames: the editor draws after the game's update and render, so no system is running over the entities. They go through `Registry::Add` and `Destroy`, so `OnAdd` and `OnRemove` hooks still run. A change the game's systems keep overwriting (a position they set every update) will not stick. The operations behind the panels (`editor/entityops.hpp`: `CreateEntity`, `SetParent`, `DestroyTree`, `DuplicateTree`) are plain functions on a `Registry` and are unit tested.

To add the editor to another game:

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
