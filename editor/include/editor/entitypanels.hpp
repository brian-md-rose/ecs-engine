#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ecs/entity.hpp"
#include "ecs/typeregistry.hpp"
#include "ecs/world.hpp"
#include "editor/editor.hpp"

namespace engine::editor
{
	// Adds the Hierarchy and Inspector panels for the entities of world, and opens them. The world must
	// have a ecs::TypeRegistry resource (the panels show a note if it has not), and must outlive the editor.
	//
	// Hierarchy: the entities as a tree by their Parent, with a filter box. Click selects, drag onto another
	// entity makes it that entity's child (a loop is refused), drag onto the "top level" line unparents, and the
	// right-click menu adds a child, duplicates or deletes (with everything under it). Inspector: the selected
	// entity's registered components, each field as a widget made from its registered kind, with Add and
	// remove for components. Changes apply at once, between frames, so they are safe while the game runs.
	//
	// What the panels have selected and typed, shared with the caller so a game can select an entity from its
	// own code, and tests can.
	struct PanelState
	{
		ecs::Entity selected = ecs::Entity::Null; // the primary selection, the one the Inspector shows
		std::vector<ecs::Entity> extra;           // the rest of a multiple selection (Ctrl or Shift click, drag box)
		std::string filter;
		std::string fieldFilter; // the Inspector's search: shows only components and fields whose name contains it
		std::string status;      // the last thing that was refused

		// The whole selection: the primary first, then the others. Empty if nothing is selected.
		[[nodiscard]] std::vector<ecs::Entity> All() const;
		[[nodiscard]] bool IsSelected(ecs::Entity entity) const;
		// Selects only entity (Null clears the selection).
		void Select(ecs::Entity entity);
		// Adds entity to the selection, making it the primary, or removes it if it was selected.
		void Toggle(ecs::Entity entity);
		// Drops dead entities, and makes one of the others primary if the primary is gone.
		void Prune(const ecs::Registry& registry);
	};

	std::shared_ptr<PanelState> AddEntityPanels(Editor& editor, ecs::World& world);

	// The same, for a world that can change from frame to frame: world is asked for it each time the panels
	// draw (the standalone editor shows the edit world, and the play world while playing).
	std::shared_ptr<PanelState> AddEntityPanels(Editor& editor, std::function<ecs::World&()> world);

	// The selected entities that have no selected ancestor: what to move or copy so that nothing moves twice (an
	// entity follows its ancestor).
	[[nodiscard]] std::vector<ecs::Entity> SelectionRoots(const ecs::Registry& registry, const PanelState& state);

	// Deletes everything selected, each with what is under it, and clears the selection.
	void DeleteSelection(ecs::Registry& registry, PanelState& state);

	// Duplicates everything selected (an entity whose ancestor is also selected goes with that ancestor's copy) and
	// selects the copies. A copy that can not be made leaves its reason in state.status.
	void DuplicateSelection(ecs::Registry& registry, const ecs::TypeRegistry& types, PanelState& state);
} // namespace engine::editor
