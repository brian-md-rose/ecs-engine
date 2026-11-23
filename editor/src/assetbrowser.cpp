#include "editor/assetbrowser.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <string>
#include <utility>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "core/text.hpp"
#include "editor/assetthumbnails.hpp"

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
			std::filesystem::path selectedFile; // tells apart two sheets that name a sprite the same
			bool grid = false;                  // sprites as a grid of pictures instead of a list
			float tileSize = 64.0F;
			SpriteSettings draft;            // the import settings being edited
			std::filesystem::path draftFile; // which sprite, and the numbers, the draft was taken from
			std::size_t draftLine = 0;
			std::string draftName;
			SpriteSettings draftBase;
			std::string saveError;
		};

		// Where the sprite is drawn inside a box: as big as fits, in whole pixels when it can (so pixel art stays
		// sharp).
		ImVec2 FitSize(float pictureWidth, float pictureHeight, float boxWidth, float boxHeight)
		{
			if (pictureWidth <= 0.0F || pictureHeight <= 0.0F) return {0.0F, 0.0F};
			float scale = std::min(boxWidth / pictureWidth, boxHeight / pictureHeight);
			if (scale >= 1.0F) scale = std::floor(scale);
			return {pictureWidth * scale, pictureHeight * scale};
		}

		// One frame of the strip (the first), or the whole strip, as an ImGui image.
		void DrawPicture(ImDrawList* draw, const AssetThumbnails::Picture& picture, ImVec2 topLeft, ImVec2 size,
		                 bool firstFrameOnly)
		{
			const float u1 = firstFrameOnly ? 1.0F / static_cast<float>(std::max(picture.frames, 1)) : 1.0F;
			draw->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(picture.texture)), topLeft,
			               ImVec2(topLeft.x + size.x, topLeft.y + size.y), ImVec2(0.0F, 0.0F), ImVec2(u1, 1.0F));
		}

		const AssetEntry* FindSelected(const AssetCatalog& catalog, const BrowserState& state)
		{
			if (state.selectedKind != ecs::AssetKind::Sprite) return nullptr;
			for (const AssetEntry& entry : catalog.Entries(ecs::AssetKind::Sprite))
				if (entry.name == state.selected && entry.file == state.selectedFile) return &entry;
			return nullptr;
		}

		void DrawGrid(const AssetCatalog& catalog, AssetThumbnails* thumbnails, BrowserState& state)
		{
			const float tile = state.tileSize;
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const float width = ImGui::GetContentRegionAvail().x;
			const int perRow = std::max(1, static_cast<int>((width + spacing) / (tile + spacing)));
			int shown = 0;
			for (const AssetEntry& entry : catalog.Entries(ecs::AssetKind::Sprite))
			{
				if (!AssetMatches(entry, state.filter)) continue;
				if (shown % perRow != 0) ImGui::SameLine();
				++shown;
				ImGui::PushID(&entry);
				const bool isSelected = state.selectedKind == ecs::AssetKind::Sprite && state.selected == entry.name &&
				                        state.selectedFile == entry.file;
				const ImVec2 topLeft = ImGui::GetCursorScreenPos();
				if (ImGui::Selectable("##tile", isSelected, 0, ImVec2(tile, tile)))
				{
					state.selected = entry.name;
					state.selectedKind = ecs::AssetKind::Sprite;
					state.selectedFile = entry.file;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s", entry.name.c_str(), entry.detail.c_str());
				const AssetThumbnails::Picture picture =
				    thumbnails != nullptr ? thumbnails->Get(entry.file, entry.name) : AssetThumbnails::Picture{};
				if (picture.texture != nullptr)
				{
					const float frameWidth = static_cast<float>(picture.width / std::max(picture.frames, 1));
					const float inner = tile - 8.0F;
					const ImVec2 size = FitSize(frameWidth, static_cast<float>(picture.height), inner, inner);
					const ImVec2 at(topLeft.x + (tile - size.x) * 0.5F, topLeft.y + (tile - size.y) * 0.5F);
					DrawPicture(ImGui::GetWindowDrawList(), picture, at, size, true);
				}
				ImGui::PopID();
			}
			if (shown == 0) ImGui::TextDisabled("No sprite matches the search");
		}

		void DrawPreview(AssetCatalog& catalog, AssetThumbnails* thumbnails, BrowserState& state)
		{
			const AssetEntry* entry = FindSelected(catalog, state);
			if (entry == nullptr)
			{
				ImGui::TextDisabled("Select a sprite to see it and its import settings");
				return;
			}
			ImGui::TextUnformatted(entry->name.c_str());
			ImGui::TextDisabled("%s", entry->detail.c_str());
			const AssetThumbnails::Picture picture =
			    thumbnails != nullptr ? thumbnails->Get(entry->file, entry->name) : AssetThumbnails::Picture{};
			if (picture.texture != nullptr)
			{
				const ImVec2 area(ImGui::GetContentRegionAvail().x, 110.0F);
				const ImVec2 size =
				    FitSize(static_cast<float>(picture.width), static_cast<float>(picture.height), area.x, area.y);
				const ImVec2 at = ImGui::GetCursorScreenPos();
				ImGui::Dummy(ImVec2(area.x, size.y));
				ImDrawList* draw = ImGui::GetWindowDrawList();
				draw->AddRectFilled(at, ImVec2(at.x + size.x, at.y + size.y), IM_COL32(40, 40, 40, 255));
				DrawPicture(draw, picture, at, size, false);
				for (int frame = 1; frame < picture.frames; ++frame)
				{
					const float x = at.x + size.x * static_cast<float>(frame) / static_cast<float>(picture.frames);
					draw->AddLine(ImVec2(x, at.y), ImVec2(x, at.y + size.y), IM_COL32(255, 255, 0, 160));
				}
				draw->AddRect(at, ImVec2(at.x + size.x, at.y + size.y), IM_COL32(255, 255, 255, 60));
			}
			else if (thumbnails != nullptr && !thumbnails->Error(entry->file).empty())
			{
				ImGui::TextColored(ImVec4(1.0F, 0.6F, 0.2F, 1.0F), "%s", thumbnails->Error(entry->file).c_str());
			}
			else if (thumbnails == nullptr)
			{
				ImGui::TextDisabled("No picture in this mode");
			}

			// Import settings: the numbers on the sprite's line of the manifest.
			const SpriteSettings& current = entry->settings;
			if (state.draftFile != entry->file || state.draftLine != entry->line || state.draftName != entry->name ||
			    !(state.draftBase == current))
			{
				state.draft = current;
				state.draftFile = entry->file;
				state.draftLine = entry->line;
				state.draftName = entry->name;
				state.draftBase = current;
				state.saveError.clear();
			}
			ImGui::SeparatorText("Import settings");
			if (entry->region)
			{
				ImGui::SetNextItemWidth(70.0F);
				ImGui::DragInt("x", &state.draft.x, 0.2F, 0, 100000);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(70.0F);
				ImGui::DragInt("y", &state.draft.y, 0.2F, 0, 100000);
			}
			ImGui::SetNextItemWidth(70.0F);
			ImGui::DragInt("cell w", &state.draft.frameWidth, 0.2F, 1, 100000);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(70.0F);
			ImGui::DragInt("cell h", &state.draft.frameHeight, 0.2F, 1, 100000);
			ImGui::SetNextItemWidth(70.0F);
			ImGui::DragInt("frames", &state.draft.frames, 0.1F, 1, 100000);
			const bool changed = !(state.draft == current);
			if (!changed) ImGui::BeginDisabled();
			if (ImGui::Button("Apply"))
			{
				state.saveError = SaveSpriteSettings(entry->file, entry->line, entry->name, state.draft);
				if (state.saveError.empty())
				{
					if (thumbnails != nullptr) thumbnails->Forget();
					catalog.Rescan(); // entry is not valid after this
					return;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Revert")) state.draft = current;
			if (!changed) ImGui::EndDisabled();
			if (!state.saveError.empty())
				ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.3F, 1.0F), "%s", state.saveError.c_str());
			ImGui::TextDisabled("Writes line %zu of %s", entry->line, core::PathText(entry->file.filename()).c_str());
		}

		void DrawTab(const AssetCatalog& catalog, AssetThumbnails* thumbnails, ecs::AssetKind kind, BrowserState& state)
		{
			const std::vector<AssetEntry>& entries = catalog.Entries(kind);
			if (entries.empty())
			{
				ImGui::TextDisabled("None found under %s", core::PathText(catalog.Folder()).c_str());
				return;
			}
			if (kind == ecs::AssetKind::Sprite && state.grid)
			{
				DrawGrid(catalog, thumbnails, state);
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
				const bool isSelected =
				    state.selectedKind == kind && state.selected == entry.name && state.selectedFile == entry.file;
				if (ImGui::Selectable(entry.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
				{
					state.selected = entry.name;
					state.selectedKind = kind;
					state.selectedFile = entry.file;
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

	std::shared_ptr<AssetCatalog> AddAssetBrowser(Editor& editor, std::filesystem::path assetsFolder,
	                                              std::shared_ptr<AssetThumbnails> thumbnails)
	{
		auto catalog = std::make_shared<AssetCatalog>(std::move(assetsFolder));
		auto state = std::make_shared<BrowserState>();
		editor.AddPanel("Assets", [catalog, state, thumbnails]
		{
			if (ImGui::Button("Rescan"))
			{
				if (thumbnails != nullptr) thumbnails->Forget();
				catalog->Rescan();
			}
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
				if (kind == ecs::AssetKind::Sprite)
				{
					ImGui::Checkbox("Grid", &state->grid);
					if (state->grid)
					{
						ImGui::SameLine();
						ImGui::SetNextItemWidth(120.0F);
						ImGui::SliderFloat("size", &state->tileSize, 32.0F, 128.0F, "%.0f");
					}
					// Beside the list, a preview and the import settings of the selected sprite.
					const bool room = ImGui::GetContentRegionAvail().x > 520.0F;
					const float listWidth = room ? ImGui::GetContentRegionAvail().x - 260.0F : 0.0F;
					if (ImGui::BeginChild("##spritelist", ImVec2(room ? listWidth : 0.0F, room ? 0.0F : 150.0F)))
						DrawTab(*catalog, thumbnails.get(), kind, *state);
					ImGui::EndChild();
					if (room) ImGui::SameLine();
					if (ImGui::BeginChild("##spritepreview", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders))
						DrawPreview(*catalog, thumbnails.get(), *state);
					ImGui::EndChild();
				}
				else
				{
					DrawTab(*catalog, nullptr, kind, *state);
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		});
		editor.SetPanelSize("Assets", 380.0F, 320.0F);
		editor.SetPanelDock("Assets", Editor::DockArea::Bottom);
		return catalog;
	}
} // namespace engine::editor
