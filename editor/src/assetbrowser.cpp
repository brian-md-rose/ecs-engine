#include "editor/assetbrowser.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <string>
#include <utility>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "core/text.hpp"

namespace engine::editor
{
	namespace
	{
		bool ContainsNoCase(const std::string& text, const std::string& needle)
		{
			const auto same = [](char a, char b)
			{
				return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			};
			return std::search(text.begin(), text.end(), needle.begin(), needle.end(), same) != text.end();
		}

		struct BrowserState
		{
			std::string filter;
			std::string selected;
			ecs::AssetKind selectedKind = ecs::AssetKind::None;
		};

		void DrawTab(const AssetCatalog& catalog, ecs::AssetKind kind, BrowserState& state)
		{
			const std::vector<AssetEntry>& entries = catalog.Entries(kind);
			if (entries.empty())
			{
				ImGui::TextDisabled("None found under %s", core::PathText(catalog.Folder()).c_str());
				return;
			}
			if (!ImGui::BeginTable("assets", 2,
			                       ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable))
				return;
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Details");
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();
			for (const AssetEntry& entry : entries)
			{
				if (!AssetMatches(entry, state.filter)) continue;
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				const bool isSelected = state.selectedKind == kind && state.selected == entry.name;
				if (ImGui::Selectable(entry.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
				{
					state.selected = entry.name;
					state.selectedKind = kind;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", core::PathText(entry.file.lexically_relative(catalog.Folder())).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(entry.detail.c_str());
			}
			ImGui::EndTable();
		}
	} // namespace

	bool AssetMatches(const AssetEntry& entry, const std::string& needle)
	{
		return needle.empty() || ContainsNoCase(entry.name, needle);
	}

	std::shared_ptr<AssetCatalog> AddAssetBrowser(Editor& editor, std::filesystem::path assetsFolder)
	{
		auto catalog = std::make_shared<AssetCatalog>(std::move(assetsFolder));
		auto state = std::make_shared<BrowserState>();
		editor.AddPanel("Assets", [catalog, state]
		{
			if (ImGui::Button("Rescan")) catalog->Rescan();
			ImGui::SameLine();
			const bool canCopy = !state->selected.empty();
			if (!canCopy) ImGui::BeginDisabled();
			if (ImGui::Button("Copy name")) ImGui::SetClipboardText(state->selected.c_str());
			if (!canCopy) ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##assetfilter", "search", &state->filter);
			if (!ImGui::BeginTabBar("assetkinds")) return;
			for (const ecs::AssetKind kind : {ecs::AssetKind::Sprite, ecs::AssetKind::Font, ecs::AssetKind::Sound})
			{
				const std::string label = std::string(AssetKindName(kind)) + " (" +
				                          std::to_string(catalog->Entries(kind).size()) + ")###" + AssetKindName(kind);
				if (!ImGui::BeginTabItem(label.c_str())) continue;
				DrawTab(*catalog, kind, *state);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		});
		editor.SetPanelSize("Assets", 380.0F, 320.0F);
		editor.SetPanelDock("Assets", Editor::DockArea::Bottom);
		return catalog;
	}
} // namespace engine::editor
