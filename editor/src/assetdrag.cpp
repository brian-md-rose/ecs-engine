#include "editor/assetdrag.hpp"

#include <algorithm>
#include <cstddef>

#include <imgui.h>

namespace engine::editor
{
	namespace
	{
		constexpr const char* kPayloadType = "ENGINE_ASSET";
	} // namespace

	DragKind DragKindFor(ecs::AssetKind kind) noexcept
	{
		if (kind == ecs::AssetKind::Font) return DragKind::Font;
		if (kind == ecs::AssetKind::Sound) return DragKind::Sound;
		return DragKind::Sprite;
	}

	std::vector<char> EncodeAssetDrag(const AssetDrag& drag)
	{
		std::vector<char> bytes;
		bytes.reserve(drag.name.size() + 1);
		bytes.push_back(static_cast<char>(drag.kind));
		bytes.insert(bytes.end(), drag.name.begin(), drag.name.end());
		return bytes;
	}

	std::optional<AssetDrag> DecodeAssetDrag(std::span<const char> bytes)
	{
		if (bytes.size() < 2) return std::nullopt; // a kind and at least one character
		const int kind = static_cast<int>(bytes[0]);
		if (kind < static_cast<int>(DragKind::Sprite) || kind > static_cast<int>(DragKind::Prefab)) return std::nullopt;
		return AssetDrag{static_cast<DragKind>(kind), std::string(bytes.begin() + 1, bytes.end())};
	}

	void AssetDragSource(const AssetDrag& drag)
	{
		if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;
		const std::vector<char> bytes = EncodeAssetDrag(drag);
		ImGui::SetDragDropPayload(kPayloadType, bytes.data(), bytes.size());
		ImGui::TextUnformatted(drag.name.c_str());
		ImGui::EndDragDropSource();
	}

	std::optional<AssetDrag> AssetDropTarget(std::initializer_list<DragKind> kinds)
	{
		if (!ImGui::BeginDragDropTarget()) return std::nullopt;
		std::optional<AssetDrag> dropped;
		// Peek first, so a payload of another kind is not accepted (and the target does not light up for it).
		if (const ImGuiPayload* peek = ImGui::GetDragDropPayload(); peek != nullptr && peek->IsDataType(kPayloadType))
		{
			const std::span<const char> bytes(static_cast<const char*>(peek->Data),
			                                  static_cast<std::size_t>(peek->DataSize));
			const std::optional<AssetDrag> held = DecodeAssetDrag(bytes);
			if (held && std::find(kinds.begin(), kinds.end(), held->kind) != kinds.end())
			{
				const ImGuiPayload* accepted = ImGui::AcceptDragDropPayload(kPayloadType);
				if (accepted != nullptr && accepted->IsDelivery()) dropped = held;
			}
		}
		ImGui::EndDragDropTarget();
		return dropped;
	}
} // namespace engine::editor
