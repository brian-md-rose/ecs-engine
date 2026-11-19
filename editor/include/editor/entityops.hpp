#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/typeregistry.hpp"

// What the hierarchy and inspector panels do to entities, as plain functions on a Registry so they can be
// tested without a window. The panels call these between frames, never inside a system.
namespace engine::editor
{
	// Entity references in the live world, for the registry's save and load: an entity's id is its index.
	// Overrides make an id stand for a different entity on load, which is how a duplicate points its
	// references at the copies.
	class LiveIds final : public ecs::IEntityIds
	{
		const ecs::Registry& registry;
		std::unordered_map<std::uint32_t, ecs::Entity> alive; // by index
		std::unordered_map<std::uint32_t, ecs::Entity> overrides;

	public:
		explicit LiveIds(const ecs::Registry& world);

		// Entity indexes (the ids) in `from` now mean the entity in `to` when loading.
		void Override(std::uint32_t id, ecs::Entity to) { overrides[id] = to; }

		[[nodiscard]] std::optional<std::int64_t> IdOf(ecs::Entity entity) const override;
		[[nodiscard]] std::optional<ecs::Entity> EntityOf(std::int64_t id) const override;
	};

	// The entity's Name, or "Entity <index>" if it has none or the name is empty.
	[[nodiscard]] std::string LabelOf(const ecs::Registry& registry, ecs::Entity entity);

	// A new entity called name, under parent unless that is null.
	ecs::Entity CreateEntity(ecs::Registry& registry, const std::string& name, ecs::Entity parent = ecs::Entity::Null);

	// True if making parent the parent of child would make a loop (parent is child, or under it).
	[[nodiscard]] bool WouldMakeLoop(const ecs::Registry& registry, ecs::Entity child, ecs::Entity parent);

	// Puts child under parent, or at the top level for a null parent. False, changing nothing, if either
	// is dead or the move would make a loop.
	bool SetParent(ecs::Registry& registry, ecs::Entity child, ecs::Entity parent);

	// Destroys the entity and everything under it, children first.
	void DestroyTree(ecs::Registry& registry, ecs::Entity entity);

	// Copies the entity and everything under it, with all their registered components. References between
	// the copied entities point at the copies; references to other entities stay as they were. The copy of
	// the root sits beside the original, and its Name gets " (copy)". Returns the copy of the root, or null
	// with the problems in issues (nothing is kept then).
	ecs::Entity DuplicateTree(ecs::Registry& registry, const ecs::TypeRegistry& types, ecs::Entity root,
	                          ecs::Issues& issues);
} // namespace engine::editor
