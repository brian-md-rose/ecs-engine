#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/json.hpp"
#include "ecs/entity.hpp"
#include "ecs/typeregistry.hpp"
#include "ecs/world.hpp"
#include "editor/assetcatalog.hpp"
#include "editor/editor.hpp"
#include "graphics/image.hpp"

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
		// What the Inspector lists for a field with FieldHints::assetKind. Null (the default): such a field shows its
		// `choices`, or a text box.
		std::shared_ptr<const AssetCatalog> assets;
		// Where a sprite's name is in the game's atlas, or nothing if the game can't draw that name. With it (and
		// `assets`) a PixelRect field marked `assetKind = Sprite` (Sprite::source) gets a picker of sprite names that
		// sets the rectangle, above the raw x, y, width and height.
		std::function<std::optional<graphics::PixelRect>(const std::string&)> spriteRect;

		// A game's own additions to the panels, set before the first frame (EditorGameConfig carries them for the
		// standalone editor). All run between frames, on the live registry.
		//
		// inspectors: a widget for one component, by its registered name, drawn under the component's header instead
		// of the field widgets (the header, its remove button and its right-click menu stay). It gets the primary
		// selected entity, which has the component, and edits it itself with ImGui and the registry. It does not take
		// part in multi-edit: the other selected entities are left alone.
		std::unordered_map<std::string, std::function<void(ecs::Registry&, ecs::Entity)>> inspectors;

		// A command in a right-click menu: run on the entity (the one clicked in the Hierarchy, or the primary
		// selection for an Inspector header) after the panel is drawn, so it may add, remove or destroy entities and
		// components.
		struct Command
		{
			std::string component; // componentCommands only: the registered name of the component whose header has it
			std::string label;
			std::function<void(ecs::Registry&, ecs::Entity)> run;
		};
		std::vector<Command> componentCommands; // in the right-click menu of that component's header in the Inspector
		std::vector<Command> entityCommands;    // in the right-click menu of an entity in the Hierarchy

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

	// Multi-edit helpers for the Inspector. Both look only at the entities that have the component `type`.
	// True if field holds different values on those entities (shown as mixed in the Inspector).
	[[nodiscard]] bool FieldIsMixed(const ecs::Registry& registry, const ecs::TypeInfo& type,
	                                const ecs::FieldInfo& field, const std::vector<ecs::Entity>& entities,
	                                const ecs::IEntityIds& ids);

	// Writes value into field on each of those entities. An entity whose Parent would make a loop is skipped. The first
	// refusal is left in error (empty if all went through); returns how many entities were changed.
	std::size_t SetFieldOnEntities(ecs::Registry& registry, const ecs::TypeInfo& type, const ecs::FieldInfo& field,
	                               const core::JsonValue& value, const std::vector<ecs::Entity>& entities,
	                               const ecs::IEntityIds& ids, std::string& error);

	// Deletes everything selected, each with what is under it, and clears the selection.
	void DeleteSelection(ecs::Registry& registry, PanelState& state);

	// Duplicates everything selected (an entity whose ancestor is also selected goes with that ancestor's copy) and
	// selects the copies. A copy that can not be made leaves its reason in state.status.
	void DuplicateSelection(ecs::Registry& registry, const ecs::TypeRegistry& types, PanelState& state);
} // namespace engine::editor
