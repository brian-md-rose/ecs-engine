#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// The startup scene a packaged game was told to start on. The editor's Build settings window writes it into
// `build.json` next to the executable (the package manifest, field "startupScene"). In Tangle Run the startup scene
// is the maze (a maze scene in assets/scenes) the game plays first; a build without a manifest, or with no startup
// scene, starts on level1 as before.
namespace game
{
	inline constexpr std::string_view kManifestFileName = "build.json";
	inline constexpr std::string_view kDefaultStartupScene = "level1";

	// The "startupScene" in the manifest file, or an empty string if the file is missing, can't be read, has no such
	// field, or the name is not a plain scene name (no folders, so a manifest can't point outside assets/scenes).
	[[nodiscard]] std::string ReadStartupScene(const std::filesystem::path& manifest);
} // namespace game
