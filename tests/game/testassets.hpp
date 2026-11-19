#pragma once

#include <filesystem>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "assets.hpp"
#include "scenes.hpp"

namespace gametest
{
	// Game assets with the scene files of the source tree (assets/scenes) and nothing else: the screens lay
	// themselves out from their scenes, so a game under test needs them to have a title menu, an options list
	// and so on. The pictures are empty, which draws nothing.
	[[nodiscard]] inline game::GameAssets SceneAssets()
	{
		std::optional<game::SceneFiles> scenes =
		    game::LoadSceneFolder(std::filesystem::path(GAME_ASSETS_DIR) / "scenes");
		REQUIRE(scenes.has_value());
		game::GameAssets assets;
		assets.scenes = std::move(*scenes);
		return assets;
	}
} // namespace gametest
