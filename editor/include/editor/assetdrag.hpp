#pragma once

#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ecs/typeregistry.hpp"

// Dragging an asset out of the Assets panel onto the Viewport or an Inspector field. The payload is the kind and the
// name, which is what the field would hold, so a drop is the same edit as picking the name from a list.
namespace engine::editor
{
	enum class DragKind
	{
		Sprite,
		Font,
		Sound,
		Scene, // a scene file name in the scene folder, with ".json"
	};

	struct AssetDrag
	{
		DragKind kind = DragKind::Sprite;
		std::string name;
	};

	// The drag kind of a field's asset kind (None counts as a sprite, which has no field of its own).
	[[nodiscard]] DragKind DragKindFor(ecs::AssetKind kind) noexcept;

	// The bytes of an ImGui drag-drop payload, and back. Decoding refuses anything that does not look like one.
	[[nodiscard]] std::vector<char> EncodeAssetDrag(const AssetDrag& drag);
	[[nodiscard]] std::optional<AssetDrag> DecodeAssetDrag(std::span<const char> bytes);

	// Call right after the item that can be dragged. Shows the name beside the pointer while dragging.
	void AssetDragSource(const AssetDrag& drag);

	// Call right after the item that takes drops. The dropped asset if one of `kinds` was released on it this frame.
	[[nodiscard]] std::optional<AssetDrag> AssetDropTarget(std::initializer_list<DragKind> kinds);

} // namespace engine::editor
