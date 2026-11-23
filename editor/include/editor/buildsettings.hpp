#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "editor/editor.hpp"

namespace engine::editor
{
	// What the Build settings window edits: which scenes ship, which one the game starts on, and where the packaged
	// game goes.
	struct BuildSettings
	{
		std::vector<std::string> scenes; // scene names (file name without ".json") that are packaged, sorted
		std::string startupScene;        // one of scenes, or empty
		std::string gameExe;             // the game executable to copy (a path, as typed)
		std::string outputFolder;        // the folder the package is written to (a path, as typed)

		friend bool operator==(const BuildSettings&, const BuildSettings&) = default;
	};

	// Where the panel finds things. All paths are as the game gives them.
	struct BuildSettingsConfig
	{
		std::filesystem::path sceneFolder;    // the scenes (.json) listed in the window
		std::filesystem::path assetFolder;    // copied into the package as `assets/`; scenes not picked are left out
		std::filesystem::path settingsFile;   // where the settings are kept between runs; empty keeps none
		std::filesystem::path defaultGameExe; // used while the settings name no executable
		std::filesystem::path defaultOutput;  // used while the settings name no output folder
		std::string gameName = "game";
		std::string engineVersion; // recorded in the manifest; may be empty
	};

	struct PackageResult
	{
		bool ok = false;
		std::string message;       // why it failed, or "Packaged N files to <folder>"
		std::size_t files = 0;     // files copied, the executable and the manifest included
		std::filesystem::path exe; // the packaged executable, when ok
	};

	// The scene names (".json" files, without the extension) in a folder, sorted. Empty if it can't be read.
	[[nodiscard]] std::vector<std::string> ListSceneNames(const std::filesystem::path& folder);

	// Reads settings written by SaveBuildSettings. A missing or unreadable file gives the defaults.
	[[nodiscard]] BuildSettings LoadBuildSettings(const std::filesystem::path& file);
	// False (and logged) if the file can't be written.
	bool SaveBuildSettings(const std::filesystem::path& file, const BuildSettings& settings);

	// What would stop a package from being made, one line each: no scenes, a startup scene that is not packaged,
	// a scene that is not in the folder, a missing executable, no output folder. Empty when it can go ahead.
	[[nodiscard]] std::vector<std::string> BuildProblems(const BuildSettings& settings,
	                                                     const std::vector<std::string>& availableScenes);

	// The manifest: game, engine version, startup scene, the scene names and every file of the package (paths
	// relative to the package, with '/', sorted).
	[[nodiscard]] core::JsonValue BuildManifest(const BuildSettingsConfig& config, const BuildSettings& settings,
	                                            const std::vector<std::string>& files);

	// Writes the package: the executable, `assets/` (every file but the scenes that are not picked), and
	// `build.json`. The package's own `assets/` is replaced; nothing else in the output folder is touched.
	// Refuses (and says why) when the settings have problems, or when the output would overwrite the source assets
	// or the executable it copies.
	[[nodiscard]] PackageResult PackageGame(const BuildSettingsConfig& config, const BuildSettings& settings);

	// Adds the Build settings panel: a checkbox per scene, a Startup pick, the executable and output paths, and
	// Package, Package and run buttons. The panel starts closed.
	void AddBuildSettingsPanel(Editor& editor, BuildSettingsConfig config);
} // namespace engine::editor
