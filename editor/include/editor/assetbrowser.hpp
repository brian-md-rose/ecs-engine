#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "editor/assetcatalog.hpp"
#include "editor/editor.hpp"

namespace engine::editor
{
	// Adds the Assets panel: the sprites, fonts and sounds found under assetsFolder (see AssetCatalog), a tab
	// each, with a search box, a Rescan button and Copy name. Give the returned catalog to PanelState::assets so
	// the Inspector's pick-lists show the same assets. The panel starts closed.
	std::shared_ptr<AssetCatalog> AddAssetBrowser(Editor& editor, std::filesystem::path assetsFolder);

	// True if the entry's name contains needle, ignoring case; an empty needle matches everything (the panel's search).
	[[nodiscard]] bool AssetMatches(const AssetEntry& entry, const std::string& needle);
} // namespace engine::editor
