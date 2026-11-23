// tangle_run_editor: Tangle Run with the Dear ImGui editor, built only when ENGINE_BUILD_EDITOR is ON
// (docs/editor.md). By default it is the standalone editor: scenes (assets/scenes) are opened, edited and saved,
// and Play runs a copy of the game on the scene. With --overlay it is the game with the editor over it (F1).
#include <SDL3/SDL_main.h>

#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "application/application.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/sdllog.hpp"
#include "core/version.hpp"
#include "editor/assetbrowser.hpp"
#include "editor/buildsettings.hpp"
#include "editor/editor.hpp"
#include "editor/editorgame.hpp"
#include "editor/entitypanels.hpp"
#include "editor/gridtool.hpp"
#include "graphics/screenshot.hpp"
#include "mazegame.hpp"
#include "mazescene.hpp"
#include "save.hpp"
#include "scenes.hpp"
#include "uidemo.hpp"

using engine::application::Application;

namespace
{

	// The folder the editor opens and saves scenes in: the source copy (game/assets/scenes, baked in by CMake) so
	// edits land where they are committed and a rebuild's asset copy can't overwrite them. Falls back to the copy next
	// to the executable when the source folder is gone (for example a built editor moved to another machine).
	std::filesystem::path SceneFolder()
	{
#ifdef TANGLE_RUN_SOURCE_SCENES
		std::error_code error;
		const std::filesystem::path source = TANGLE_RUN_SOURCE_SCENES;
		if (std::filesystem::is_directory(source, error)) return source;
#endif
		return engine::core::ExecutableDirectory() / "assets" / "scenes";
	}

	// The assets folder next to the scenes (source copy or the one beside the executable): what the Assets panel lists.
	std::filesystem::path AssetFolder()
	{
		return SceneFolder().parent_path();
	}

	// A new copy of the game for the edit world or for Play. Its scene files are read again from the folder the editor
	// saves to, so Play shows an edited screen without a rebuild copying the assets first.
	std::unique_ptr<engine::application::ScheduledGame> MakeEditedGame()
	{
		auto made = std::make_unique<game::MazeGame>();
		if (!made->IsValid()) return nullptr;
		// The editor makes several games and throws them away; none may write the player's save file.
		made->world.resources.Get<game::SaveFile>().path.clear();
		// A scene that can't be read leaves the copy's own scenes in place (the error is logged).
		if (std::optional<game::SceneFiles> scenes = game::LoadSceneFolder(SceneFolder()))
			made->world.resources.Get<game::GameScenes>().files = std::move(*scenes);
		return made;
	}

	void ResolveAfterLoad(engine::ecs::World& world)
	{
		const auto* clips = world.resources.TryGet<game::SpriteClips>();
		if (clips == nullptr) return;
		game::ResolveClipNames(world.registry, *clips);
		// A maze scene: put the tiles where their cells say and give them their pictures, even if the maze is
		// not valid yet.
		if (game::HasSceneMaze(world.registry))
		{
			game::PlaceMazeTiles(world.registry);
			game::ResolveMazeTiles(world.registry, *clips);
		}
	}

	// Where a sprite of sprites.sheet is in the atlas of the world's game, for the Sprite component's source picker.
	std::optional<engine::graphics::PixelRect> SpriteRect(engine::ecs::World& world, const std::string& name)
	{
		const auto* clips = world.resources.TryGet<game::SpriteClips>();
		if (clips == nullptr) return std::nullopt;
		const engine::graphics::Clip* clip = game::FindClip(*clips, name);
		if (clip == nullptr || clip->frames == 0) return std::nullopt;
		return clip->firstFrame;
	}

	// The maze painter: the tile palette and grid for a maze scene, and nothing for any other scene.
	engine::editor::GridTool MazePainter()
	{
		engine::editor::GridTool tool;
		tool.grid = [](engine::ecs::World& world) -> std::optional<engine::editor::GridInfo>
		{
			std::optional<engine::editor::GridInfo> grid;
			world.registry.Each<game::MazeLayout>([&](engine::ecs::Entity, const game::MazeLayout& layout)
			{
				grid = engine::editor::GridInfo{.columns = layout.columns,
				                                .rows = layout.rows,
				                                .x = layout.x,
				                                .y = layout.y,
				                                .cellWidth = game::kTileSize,
				                                .cellHeight = game::kTileSize};
			});
			return grid;
		};
		tool.palette = game::MazePaletteNames();
		tool.paint = [](engine::ecs::World& world, std::size_t kind, engine::editor::GridCell cell)
		{
			game::PaintMazeTile(world.registry, kind, cell.column, cell.row);
		};
		tool.erase = [](engine::ecs::World& world, engine::editor::GridCell cell)
		{
			game::EraseMazeTile(world.registry, cell.column, cell.row);
		};
		tool.refresh = ResolveAfterLoad;
		tool.problems = [](engine::ecs::World& world)
		{
			return game::BuildTileMap(world.registry).error.message;
		};
		return tool;
	}

	// Play on a maze scene starts the game on that maze; any other scene is left to the game's own start.
	std::string StartPlay(engine::ecs::World& world)
	{
		return game::HasSceneMaze(world.registry) ? game::AdoptSceneMaze(world) : std::string();
	}

	// Play from here: the player starts on the clicked cell of the maze.
	std::string StartPlayFrom(engine::ecs::World& world, engine::editor::GridCell cell)
	{
		if (!game::HasSceneMaze(world.registry)) return "this scene has no maze";
		return game::PlacePlayerStart(world, cell.column, cell.row);
	}
} // namespace

int main(int argc, char** argv)
{
	const std::filesystem::path logFolder =
	    engine::core::SaveDirectory(game::kSaveOrganisation, game::kSaveApplication);
	const engine::core::LogSession logging(
	    {.file = logFolder.empty() ? std::filesystem::path() : logFolder / "editor-log.txt"});
	engine::core::RouteSdlLog();

	bool overlay = false;
	std::string openScene; // the first argument that is not a flag: a scene in the scene folder
	for (int i = 1; i < argc; ++i)
	{
		if (const int width = engine::graphics::ScreenshotArgWidth(argv[i]); width > 0)
		{
			i += width - 1; // a screenshot option and its value, read by Application::SetScreenshotArgs
			continue;
		}
		if (std::strcmp(argv[i], "--overlay") == 0)
			overlay = true;
		else if (argv[i][0] != '-' && openScene.empty())
			openScene = std::filesystem::path(argv[i]).filename().string(); // only the scene folder is read
	}

	Application app(game::MazeGame::kConfig);
	app.SetScreenshotFolder(engine::graphics::ScreenshotDirectory(logFolder));
	app.SetScreenshotArgs(engine::graphics::ParseScreenshotArgs(argc, argv));
	engine::editor::Editor editor;
	// Panel positions and the scenes last opened are kept with the log, so the next start picks up where this left off.
	if (!logFolder.empty()) editor.SetLayoutFile(logFolder / "editor-layout.ini");

	if (overlay)
	{
		game::MazeGame game;
		if (!game.IsValid()) return 1;
		const auto panels = engine::editor::AddEntityPanels(editor, game.world);
		panels->assets = engine::editor::AddAssetBrowser(editor, AssetFolder());
		panels->spriteRect = [&game](const std::string& name)
		{
			return SpriteRect(game.world, name);
		};
		app.SetEditorHost(&editor);
		return app.Run(game);
	}

	engine::editor::EditorGame game(
	    editor, {.makeGame = MakeEditedGame,
	             .afterLoad = ResolveAfterLoad,
	             .sceneFolder = SceneFolder(),
	             .assetFolder = AssetFolder(),
	             .spriteRect = SpriteRect,
	             .recentFile = logFolder.empty() ? std::filesystem::path() : logFolder / "editor-recent.txt",
	             .autosaveFolder = logFolder.empty() ? std::filesystem::path() : logFolder / "autosave",
	             .openOnStart = openScene,
	             .onPlay = StartPlay,
	             .onPlayFrom = StartPlayFrom,
	             .gridTool = MazePainter()});
	if (!game.IsValid()) return 1;
	// Build settings: the game exe is the one built beside the editor (game.exe in the same configuration folder).
	const std::filesystem::path exeFolder = engine::core::ExecutableDirectory();
	engine::editor::AddBuildSettingsPanel(
	    editor, {.sceneFolder = SceneFolder(),
	             .assetFolder = AssetFolder(),
	             .settingsFile = logFolder.empty() ? std::filesystem::path() : logFolder / "editor-build.json",
	             .defaultGameExe = exeFolder / "game.exe",
	             .defaultOutput = exeFolder / "package" / "Tangle Run",
	             .gameName = game::MazeGame::kConfig.title,
	             .engineVersion = engine::kVersionString});
	editor.SetVisible(true);
	app.SetEditorHost(&editor);
	return app.Run(game);
}
