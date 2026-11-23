#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "editor/assetcatalog.hpp"
#include "editor/assetthumbnails.hpp"
#include "editor/editor.hpp"

namespace engine::editor
{
	// Adds the Assets panel: the sprites, fonts and sounds found under assetsFolder (see AssetCatalog), a tab
	// each, with a search box, a Rescan button and Copy name. Give the returned catalog to PanelState::assets so
	// the Inspector's pick-lists show the same assets. The panel starts closed.
	// The Sprites tab can show a grid of pictures and, for the selected sprite, a preview and its import settings
	// (the numbers on its manifest line, which Apply writes back). The pictures need `thumbnails`, whose renderer the
	// game sets (EditorGame does); without it the preview shows the settings only.
	// With a sceneFolder, a Scenes tab lists its .json files. Every row (a sprite, font, sound or scene) can be
	// dragged: onto the Viewport (a sprite makes an entity, a scene opens) or an Inspector field that takes that kind
	// of asset.
	std::shared_ptr<AssetCatalog> AddAssetBrowser(Editor& editor, std::filesystem::path assetsFolder,
	                                              std::shared_ptr<AssetThumbnails> thumbnails = nullptr,
	                                              std::filesystem::path sceneFolder = {});

	// True if the entry's name contains needle, ignoring case; an empty needle matches everything (the panel's search).
	[[nodiscard]] bool AssetMatches(const AssetEntry& entry, const std::string& needle);
} // namespace engine::editor
