# The editor overlay

`engine::editor` is a [Dear ImGui](https://github.com/ocornut/imgui) editor for the engine. It is an overlay drawn on top of a running game (the hook and shell, Hierarchy and Inspector panels), and also the standalone editor with scene files, Play and Stop, a Viewport and undo and redo, all described below. It is off by default, so a game that doesn't want it never compiles it and `engine::platform` doesn't depend on it.

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

## The standalone editor

`tangle_run_editor` starts as a standalone editor (`--overlay` gives the F1 overlay on the running game instead). `EditorGame` (`editor/editorgame.hpp`) is the `Game` it runs. It holds two copies of the game, both made by `EditorGameConfig::makeGame`:

- The **edit world** has the game's resources and systems but runs none of them. It holds the scene being edited, and the Hierarchy and Inspector work on it.
- The **play world** exists only between Play and Stop. Play saves the edit world's entities, makes a fresh game, loads them into it and runs it. Stop throws it away, so nothing done while playing reaches the scene.

The **Scene** menu has New, Open (the `.json` files in `sceneFolder`; Tangle Run's editor uses the source folder `game/assets/scenes`, so saves are committed with the repository, and falls back to `assets/scenes` next to the executable if that folder is missing), Save and Save as. A `*` beside the name marks changes not saved; New and Open ask before discarding them. Open reads the file first and keeps the current scene if it has errors. `makeGame` must not let a game write the player's save file (Tangle Run's factory clears `SaveFile::path`), since games are made and discarded freely. `afterLoad` runs after each load, for what a game does with its entities after loading (Tangle Run resolves sprite clip names).

Saved scenes go to the source folder, but Play and the game read the copy next to the executable, so an edited screen scene (title, pause and so on) only changes in Play after the next build copies the assets again.

Limit: Play runs the game's own systems over the scene's entities, and a game that also spawns entities when a state is entered does that on top of the scene. Tangle Run's menu screens are scene files that their enter systems load with `SpawnScene`, and its playing field (maze, pellets, enemies, HUD) is still built from code. Play is most useful for scenes the game loads whole, such as `uidemo.json`.

## The viewport

In the standalone editor the game is drawn into a picture (a render target as big as the game's logical screen, `Renderer::CreateRenderTarget` and `SetRenderTarget`) and the **Viewport** panel shows it. The mouse wheel zooms about the pointer, middle or right drag pans, and Reset view puts it back; this moves the view of the picture, not the game's own `Camera`. A click selects the entity drawn under it (`PickEntity` in `editor/viewport.hpp`: the highest layer, then the later entity), and the selected entity has a yellow outline. Only things with a `Shape`, a `Sprite` or a UI `Hitbox` can be picked, since those are the ones whose size is known; text and nine-patches are not yet. World-space entities go through the game's camera (`Renderer2D::camera`), and `ScreenSpace` ones, or their children, do not. If a renderer can't make a render target, the game is drawn straight to the window instead and the Viewport panel stays empty.

While playing, the game's mouse input is not mapped into the viewport: use the keyboard, or run the game itself. Dragging an entity in the viewport moves it (its `LocalTransform` if it has a parent, otherwise its `Transform`; scaled by the camera zoom for world-space entities).

## Undo and redo

**Edit > Undo** (Ctrl+Z) and **Redo** (Ctrl+Y, or Ctrl+Shift+Z) step through the changes made to the scene, up to 100, in the standalone editor. A step is a finished change: a whole drag in the viewport, a value typed into the Inspector until the box is left, one menu click (add child, duplicate, delete, add or remove a component). It is found by looking at the scene when the pointer and widgets go quiet, so everything the editor can change is covered, with no changes needed in the panels. New and Open start the list again, and there is no undo while playing. A text box being typed in keeps its own Ctrl+Z.

This differs from the plan's per-component commands: each step is the whole scene as text (`SaveScene`), and undoing loads the neighbouring one. It is simple and covers creating and deleting entities too, at the cost of memory for large scenes and of entity ids changing on each undo (the selection is kept by its position in the scene).

## How it hooks in

`application/editorhost.hpp` defines `EditorHost`, with no SDL types (the window, renderer and events cross as opaque pointers). `Application::SetEditorHost` takes one, not owned, and then:

- calls `Attach` just before the main loop and `Detach` after it;
- passes every SDL event to `OnEvent` first, and skips the game's own handling when it returns true (a close request is never taken);
- calls `Draw` after the game's `Render` and before the frame is presented;
- asks `WantsSystemCursor` each frame.

With no host set, none of this costs anything. The overlay draws at real window pixels, not the game's logical size, so its text stays sharp: `Editor::Draw` turns the renderer's logical presentation off for the overlay's draw calls and back on after. It writes no `imgui.ini`.

ImGui is a submodule pinned to a release tag (v1.92.9b); to update it, check out another tag in `thirdparty/imgui` and commit.
