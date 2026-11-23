# Scenes and the type registry

A scene is a set of entities and their components saved as JSON, so a game can describe its starting
entities in data instead of spawn code. It is built on the **type registry**, which describes component
types at run time. Both live in `engine::core`: no SDL, no window, no editor. Design background is in
[ADR 0004](decisions/0004-reflection-and-scenes.md).

## Registering components

C++ has no reflection, so each component is described once, in a block next to the component:

```cpp
#include "ecs/enginecomponents.hpp"
#include "ecs/typeregistry.hpp"

void RegisterGameComponents(engine::ecs::TypeRegistry& types)
{
    types.Register<Follow>("Follow")
        .Field("target", &Follow::target)                    // an Entity, saved as an id
        .Field("speed", &Follow::speed, {.min = 0, .max = 10});
    types.Register<Mood>("Mood")
        .EnumField("mood", &Mood::value, {{"Calm", Calm}, {"Angry", Angry}});
    types.RegisterTag<Enemy>("Enemy");                        // no fields, saved as {}
}

engine::ecs::TypeRegistry types;
engine::ecs::RegisterEngineComponents(types);  // Name, Parent, Transform, Shape, Sprite, ...
RegisterGameComponents(types);
```

- The name is what a saved file calls the component. **Renaming it breaks old files.**
- A field type needs a `FieldCodec<T>`. Built in: `bool`, integers (range checked), `float` and `double`
  (finite only), `std::string`, `Entity`, `std::vector<Entity>`, enums by name (`EnumField`), and `Color`,
  `Position`, `Scale` and `PixelRect` (from `ecs/enginecomponents.hpp`). A type with no codec is a compile error, so
  a pointer or `std::function` can't be saved by accident. Specialise `FieldCodec` for your own value types.
- Leave out members that are caches, or register the component with `.Runtime()`: it is then never saved and a
  scene that names it gets a warning.
- A component must be default-constructible. A field missing from a file keeps its default.
- **Keep the block honest:** a component that gains a field needs its block updated. A test that round-trips
  one of each registered component through JSON catches a field that can't be written or read back.

`TypeInfo` also offers `Has`, `AddDefault`, `Remove` and `Get` for code that doesn't know the type (an
inspector), and `Save` and `Load` for one component.

## The scene file

```json
{
  "format": 1,
  "engine": "0.6.0",
  "entities": [
    { "id": 1, "name": "Player",
      "components": { "Transform": { "position": { "x": 16.0, "y": 24.5 } } } },
    { "id": 2, "parent": 1, "components": { "Follow": { "target": 1, "speed": 0.5 } } }
  ]
}
```

- `id` is the entity's number **in the file**. Entity-valued fields and `parent` are written as ids and
  remapped when loading, so entities can refer to ones later in the file. The ids are numbered from 1 in the
  order of the entities' own ids, so a save of the same world is the same text and diffs cleanly.
- `name` is the `Name` component and `parent` the `Parent` component; neither appears under `components`.
- `format` is the file layout version (1). A file with another number is refused. `engine` is for diagnostics.
- Resources (the maze, the score) are not part of a scene. Prefabs are not supported yet.
- `Transform` and `LocalTransform` also have `rotation` (degrees, clockwise on screen) and `scale` (`{"x", "y"}`, default 1). A scene written before they existed omits them and loads unchanged, as every missing field keeps its default; saving now writes them. Shapes and sprites rotate and scale about their top-left corner (the `position`); text ignores both. A child gets its parent's rotation added and its scale multiplied, and its `position` is stretched and turned by the parent's first.

## Scene settings

A scene may also say how it is drawn, in an optional `settings` object beside `entities`. Both parts are optional and a reader that does not know them ignores them, so the format number stays 1:

```json
"settings": { "camera": { "x": 0, "y": 0, "zoom": 2 }, "background": [20, 20, 40] }
```

`ecs::SceneSettings` is that object as a value (`ReadSceneSettings` and `WriteSceneSettings` convert it; `LoadSceneFile` and `SaveSceneFile` take an optional pointer). A wrong-shaped part is left out with a warning, so it never stops a scene from loading. The engine only reads and writes it: a game keeps the value as a resource and applies it. Tangle Run's `SpawnScene` sets the resource from a scene that has settings (one without leaves it as it was), and its render system moves the camera and clears to the background colour. The logical resolution is fixed when the game starts, so it is not a scene setting.

## Saving and loading

```cpp
engine::ecs::Issues issues;
engine::ecs::SaveSceneFile(registry, types, "level.json", issues);

const auto result = engine::ecs::LoadSceneFile(registry, types, "level.json");
if (!result.Ok())
{
    for (const auto& error : result.issues.errors) Log(error.path + ": " + error.message);
}
// result.entities: the entities created, in file order.
```

- **Loading adds entities to the registry you give it**, through `Registry::Add`, so `OnAdd` hooks run.
- Problems are collected, not thrown, each with a path such as `entities[3].components.Follow.target`.
  A component name that isn't registered, a field that isn't known and a `Runtime` component in a file are
  **warnings** (an old scene survives a removed component). Anything else is an error.
- If there is any error the load is undone: every entity it created is destroyed again, and none is returned. (Side effects of `OnAdd` hooks are not undone.) A `parent` chain that loops is an error.
- `SaveScene` can also take a list of entities (a selection). A reference from one of them to an entity outside
  the list is an error.
- A component the entity already has is replaced when loaded onto it by `TypeInfo::Load`; a scene load always
  creates new entities.

## Limits

Entities only. There are no resources, prefabs or asset references yet; textures and sounds belong in
scenes as names, resolved after loading.

## Example: Tangle Run's screens

The UI demo screen (OPTIONS, then UI DEMO) is built from `game/assets/scenes/uidemo.json`, loaded by
`SpawnUiDemoScreen` when the screen opens. The file holds the panel, title, counter, three buttons and their
labels, the list's frame, view and content, the two hint lines and the mouse cursor. The code still:

- adds `DespawnOnExit<Screen>` to every entity the scene created, so they go with the screen;
- fills in each picture's atlas rectangle. A scene can't hold those (they depend on the packed atlas), so it
  names the picture with the game's `ClipName` component (`"panel"`, `"button"`, `"cursor"`) and
  `ResolveClipNames` looks the rectangle up after loading. This is how a scene refers to assets;
- spawns the 30 text rows of the scrolling list under the scene's "List content" entity, since each row's
  position depends on how tall the ones before it wrap, and sets the scroll view's content height.

The game registers the engine's components and `RegisterGameComponents` (`scenes.hpp`: the demo's and the menu screens' own) in a `TypeRegistry`
resource when it is built, and `LoadGameAssets` parses the scene files (every `.json` in `assets/scenes`) with the other assets. If the file is
missing or broken the game doesn't start (as for any asset); if the scene itself fails to load, the screen opens
empty and the errors, with their paths, go to the log.

The title, how-to-play, options, name entry, level transition and pause screens are laid out the same way, by `title.json`, `help.json`, `options.json`, `nameentry.json`, `leveltransition.json` and `pause.json`, with their changing text written by `ScreenTextSystem` (see [screens.md](screens.md#layouts-are-scenes)). The playing field (maze, pellets, enemies, HUD) is still built from code.
