// tangle_run_editor: Tangle Run with the Dear ImGui editor, built only when ENGINE_BUILD_EDITOR is ON
// (docs/editor.md). By default it is the standalone editor: scenes (assets/scenes) are opened, edited and saved,
// and Play runs a copy of the game on the scene. With --overlay it is the game with the editor over it (F1).
#include <SDL3/SDL_main.h>

#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <system_error>
#include <utility>

#include "application/application.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/sdllog.hpp"
#include "editor/editor.hpp"
#include "editor/editorgame.hpp"
#include "editor/entitypanels.hpp"
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

	// Play on a maze scene starts the game on that maze; any other scene is left to the game's own start.
	void StartPlay(engine::ecs::World& world)
	{
		if (game::HasSceneMaze(world.registry)) (void)game::AdoptSceneMaze(world);
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
	for (int i = 1; i < argc; ++i)
		if (std::strcmp(argv[i], "--overlay") == 0) overlay = true;

	Application app(game::MazeGame::kConfig);
	engine::editor::Editor editor;

	if (overlay)
	{
		game::MazeGame game;
		if (!game.IsValid()) return 1;
		engine::editor::AddEntityPanels(editor, game.world);
		app.SetEditorHost(&editor);
		return app.Run(game);
	}

	engine::editor::EditorGame game(editor, {.makeGame = MakeEditedGame,
	                                         .afterLoad = ResolveAfterLoad,
	                                         .sceneFolder = SceneFolder(),
	                                         .onPlay = StartPlay});
	if (!game.IsValid()) return 1;
	editor.SetVisible(true);
	app.SetEditorHost(&editor);
	return app.Run(game);
}
