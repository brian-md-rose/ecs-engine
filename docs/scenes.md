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
  `Position` and `PixelRect` (from `ecs/enginecomponents.hpp`). A type with no codec is a compile error, so
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
- If there is any error the load is undone: every entity it created is destroyed again, and none is returned.
- `SaveScene` can also take a list of entities (a selection). A reference from one of them to an entity outside
  the list is an error.
- A component the entity already has is replaced when loaded onto it by `TypeInfo::Load`; a scene load always
  creates new entities.

## Limits

Entities only. There are no resources, prefabs or asset references yet; textures and sounds belong in
scenes as names, resolved after loading.
