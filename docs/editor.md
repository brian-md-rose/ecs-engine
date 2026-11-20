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

- **Hierarchy:** every entity as a tree by `Parent`, labelled by its `Name` (or `Entity <index>`), with a filter box. Click selects; Ctrl or Shift click adds an entity to the selection (or takes it out), and Duplicate, Delete and the Viewport's moves act on all of it. The Inspector shows the first one picked. Drag an entity onto another to make it a child, or onto the "top level" line to unparent it; a move that would make an entity its own ancestor is refused. Right-click for *Add child*, *Duplicate* (the entity and everything under it, with references between them pointing at the copies) and *Delete* (with everything under it). *New entity* makes one at the top level.
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

The **Scene** menu has New, Open (the `.json` files in `sceneFolder`; Tangle Run's editor uses the source folder `game/assets/scenes`, so saves are committed with the repository, and falls back to `assets/scenes` next to the executable if that folder is missing), Open recent (the last eight scenes opened or saved), Save and Save as. A `*` beside the name marks changes not saved; New and Open ask before discarding them. Open reads the file first and keeps the current scene if it has errors. `makeGame` must not let a game write the player's save file (Tangle Run's factory clears `SaveFile::path`), since games are made and discarded freely. `afterLoad` runs after each load, for what a game does with its entities after loading (Tangle Run resolves sprite clip names).

The **Issues** panel lists every problem the last open, save or Play start reported (errors first, then warnings, each with its JSON path such as `entities[3].components.Follow.target`), where the menu bar only has room for the first; it opens by itself when there is something to list, and New or Clear empties it. An unknown component name in a file is a warning, so a scene survives a removed component, and the panel is where you see which.

The editor remembers the last scene and the panel layout between runs: `EditorGameConfig::recentFile` names a text file of scene names, newest first, and the editor reopens the first one at start (if it is gone you get a new scene and no complaint); `Editor::SetLayoutFile` is where Dear ImGui keeps the panels' positions and sizes. Tangle Run's editor keeps both next to its log (`editor-recent.txt` and `editor-layout.ini` in the save folder). `EditorGameConfig::openOnStart` opens a named scene instead of the last one; `tangle_run_editor title.json` (or `title`) passes its first argument that is not a flag, using only the file name, so it must be in the scene folder. A scene that can't be opened is reported in the menu bar and the editor starts on a new scene.

Saved scenes go to the source folder, and each game the editor makes (the edit world, and the copy for each Play) reads its scene files from that folder again, so an edited screen scene (title, pause and so on) shows in Play straight away. The game itself still reads the copy next to the executable.

`EditorGameConfig::onPlay` is called on the play world after the scene is loaded, so a game can start itself on what the scene holds; Tangle Run uses it to start on its maze when the scene is a maze scene (`level1.json`, see [maze format](maze-format.md#maze-scenes)): the game plays on the scene's tiles instead of spawning its own. Otherwise Play runs the game's own systems over the scene's entities, and a game that also spawns entities when a state is entered does that on top of the scene. Tangle Run's menu screens are scene files that their enter systems load with `SpawnScene`, and its playing field (maze, pellets, enemies, HUD) is still built from code. Play is most useful for scenes the game loads whole, such as `uidemo.json`.

## The viewport

In the standalone editor the game is drawn into a picture (a render target as big as the game's logical screen, `Renderer::CreateRenderTarget` and `SetRenderTarget`) and the **Viewport** panel shows it. The mouse wheel zooms about the pointer, middle or right drag pans, and Reset view puts it back; this moves the view of the picture, not the game's own `Camera`. A click selects the entity drawn under it (Ctrl or Shift click adds to the selection or removes from it, and a press on empty space drags a box that selects what it covers; dragging any selected entity moves the whole selection, and Delete, Ctrl+D and the arrow keys act on all of it) (`PickEntity` in `editor/viewport.hpp`: the highest layer, then the later entity), and the selected entity has a yellow outline. Things with a `Shape`, a `Sprite`, a UI `Hitbox`, a `NinePatch` or a `Text` can be picked. A text's size is measured with the engine's default font metrics (6 by 8 pixels a character), so it is exact for the game's own font. Selecting an entity from the Hierarchy scrolls the view to it if it is off-screen. World-space entities go through the game's camera (`Renderer2D::camera`), and `ScreenSpace` ones, or their children, do not. If a renderer can't make a render target, the game is drawn straight to the window instead and the Viewport panel stays empty.

While playing, the game gets the mouse over the viewport (`PlayMouse`, `editor/viewport.hpp`): positions are in the picture's pixels, a press that began over the viewport stays with the game until it is released, and the wheel and buttons go to the game, so hold Ctrl to zoom and pan the view instead. Clicking and dragging do not select or move entities while playing. Dragging an entity in the viewport moves it. **Snap** (next to Reset view) rounds the result to multiples of the grid size beside it; holding Ctrl while dragging flips the setting, and holding Shift keeps only the larger of the two movements. The arrow keys nudge the selection one unit, or one grid step with Shift (only while the Viewport has the pointer or focus, and not while typing in a text box; with Snap on a nudge is one grid step and lands on the grid). What moves is its `LocalTransform` if it has a parent, otherwise its `Transform`, scaled by the camera zoom for world-space entities.

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

## Shortcuts

In the standalone editor: Ctrl+S Save, Ctrl+Shift+S Save as, Ctrl+N New, Ctrl+Z Undo, Ctrl+Y (or Ctrl+Shift+Z) Redo, Ctrl+C copy and Ctrl+V paste the selected entities (each with what is under it, as scene text on the clipboard, so a paste can go into another scene or be saved in a text file; it fails with a message if a copied entity refers to one that was not copied), Ctrl+D duplicate the selected entity, Delete delete it and everything under it, F5 Play or Stop. While playing only F5 works. None of them act while a text box has focus, a drag is in progress or a dialog is open.

## Painting a grid scene

A game can give the standalone editor a `GridTool` (`editor/gridtool.hpp`, through `EditorGameConfig::gridTool`): a palette of tile kinds and functions to paint and erase a cell. When the scene being edited has a grid (Tangle Run's maze scenes, `game/assets/scenes/level1.json`), the Viewport shows a row of radio buttons: **Select** (the usual click and drag), one for each palette entry (Wall, Door, Pellet, Power pellet, Player start, Enemy start, Tunnel) and **Erase**. With a brush chosen, the grid is drawn over the picture, the cell under the pointer is outlined, and pressing and dragging with the left button paints every cell the pointer crosses (`CellLine`, so a fast drag leaves no gaps). Painting a cell replaces what is in it, painting what is already there changes nothing, and a maze has one player start, so painting a second moves it. After each frame that painted, the game's `refresh` runs (for the maze it places the tiles and joins the walls up again). A whole stroke is one undo step. What is wrong with the maze as it stands (no player start, a tunnel without its pair) is shown in orange above the picture, and Play refuses to start on a maze that isn't valid, saying why in the menu bar.

The grid is laid over the picture at the game's own pixels, so it is right for a game whose camera is at its default (Tangle Run's is). The brush is off while playing.
