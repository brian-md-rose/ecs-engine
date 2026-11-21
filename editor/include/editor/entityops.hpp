#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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

	// The roots and everything under them as scene JSON text, for the clipboard. An entity whose Parent is not
	// among them is saved without one. References from the copied entities to others outside them are errors
	// in issues, and the text is then empty.
	[[nodiscard]] std::string CopyTrees(ecs::Registry& registry, const ecs::TypeRegistry& types,
	                                    std::span<const ecs::Entity> roots, ecs::Issues& issues);

	// Creates the entities of text (from CopyTrees, possibly in another scene) at the top level, and returns the
	// ones that have no parent in the text, so the pasted trees' roots. Empty, with the problems in issues and
	// nothing kept, if text is not a scene or does not load.
	[[nodiscard]] std::vector<ecs::Entity> PasteTrees(ecs::Registry& registry, const ecs::TypeRegistry& types,
	                                                  std::string_view text, ecs::Issues& issues);

	// The entities of a registry as a tree for the hierarchy panel.
	struct Tree
	{
		std::vector<ecs::Entity> roots;                                     // shown at the top level
		std::unordered_map<ecs::Entity, std::vector<ecs::Entity>> children; // by parent
		std::vector<ecs::Entity> all;                                       // every live entity, by index
	};

	// Entities with no live parent are roots. An entity that can't be reached from any root, because its Parent
	// chain loops (from a hand-edited scene, say), is made a root too, so it can still be selected and fixed.
	[[nodiscard]] Tree BuildTree(const ecs::Registry& registry);

	// True if text contains filter, ignoring case. An empty filter matches everything.
	[[nodiscard]] bool ContainsNoCase(const std::string& text, const std::string& filter);

	// The values of one component of entity as JSON text ({"type": ..., "fields": {...}}), for the clipboard.
	// Empty, with the reason in error, if the entity lacks the component or a field can't be saved.
	[[nodiscard]] std::string CopyComponent(const ecs::Registry& registry, const ecs::TypeInfo& type,
	                                        ecs::Entity entity, std::string& error);

	// Puts text (from CopyComponent) onto entity: adds the component if it has none, then loads every field the
	// text holds. Fields it does not hold keep their values. False, with the reason in error, if text is not a
	// component copy, names a type that is not registered, or a field does not load (nothing is changed then).
	[[nodiscard]] bool PasteComponent(ecs::Registry& registry, const ecs::TypeRegistry& types, ecs::Entity entity,
	                                  std::string_view text, std::string& error);

	// True if the entity's label contains filter, ignoring case. An empty filter matches everything.
	[[nodiscard]] bool MatchesFilter(const ecs::Registry& registry, ecs::Entity entity, const std::string& filter);

	// True if the entity or anything under it matches the filter, so a match is shown with its ancestors.
	[[nodiscard]] bool SubtreeMatches(const ecs::Registry& registry, const Tree& tree, ecs::Entity entity,
	                                  const std::string& filter);
} // namespace engine::editor
