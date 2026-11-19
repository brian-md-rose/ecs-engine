#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ecs/entity.hpp"
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
		ecs::Entity selected = ecs::Entity::Null;
		std::string filter;
		std::string status; // the last thing that was refused
	};

	std::shared_ptr<PanelState> AddEntityPanels(Editor& editor, ecs::World& world);

	// The same, for a world that can change from frame to frame: world is asked for it each time the panels
	// draw (the standalone editor shows the edit world, and the play world while playing).
	std::shared_ptr<PanelState> AddEntityPanels(Editor& editor, std::function<ecs::World&()> world);
} // namespace engine::editor
