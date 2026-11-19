# 0004 – Reflection and scenes

**Status:** accepted · **Phase:** editor plan E1 and E2

## Context

The engine had no scene format and no way to look inside a component, so a game described its starting
entities in spawn code, and an editor (see `plans/editor-plan.md`) had nothing to show or save. Component
types exist only at compile time, and C++20 has no reflection.

## Decision

- **An opt-in `TypeRegistry` in `engine::core`** (`ecs/typeregistry.hpp`). Each component is registered once,
  explicitly, with a name and a list of fields. Registration stores closures over `Registry::Add`,
  `TryGetComponent` and `Remove`, so `Registry` and `ComponentPool` are untouched and a game that never
  registers a type pays nothing. A macro or compile-time helper can come later, once the shape has settled.
- **`FieldCodec<T>` decides what can be saved.** A field type with no codec doesn't compile, so pointers,
  `std::function` and unregistered containers are rejected, not guessed. `Runtime` components are never saved.
- **Scenes are JSON** (`ecs/scene.hpp`, `core::JsonValue`): ordered keys and a deterministic writer, so a scene
  diffs in git, and no new dependency.
- **Entities are saved as scene-local ids**, never as `Entity`, and remapped on load in a second pass, so order
  doesn't matter. A reference to an entity outside the scene is an error with a path.
- **Loading goes through `Registry::Add`**, so `OnAdd` hooks run. Errors are collected with paths; an
  unknown component is only a warning; any error undoes the whole load.
- **`name` and `parent` are top-level keys** that map to the `Name` and `Parent` components.
- **The file has an integer `format`**, refused if it isn't the one this engine reads. Migrations are added
  when a second format exists.
- **Entities only for now.** Resources and prefabs are left out; the format can grow a `"prefab"` key later.

## Consequences

- A component gains a field, its registration block must follow. A round-trip test per registered component
  keeps them honest.
- Ids are numbered by position on every save, so an entity's id can change when others are added before it.
  Nothing outside the file should hold an id.
- A float is written in its shortest form, so `0.1f` is `0.1`, and read back to the same float.
- The editor (ImGui overlay, E3 onwards) builds on this layer, and a game can use scenes without it.
