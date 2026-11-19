// tangle_run_editor: Tangle Run with the Dear ImGui editor, built only when ENGINE_BUILD_EDITOR is ON
// (docs/editor.md). By default it is the standalone editor: scenes (assets/scenes) are opened, edited and saved,
// and Play runs a copy of the game on the scene. With --overlay it is the game with the editor over it (F1).
#include <SDL3/SDL_main.h>

#include <cstring>
#include <memory>

#include "application/application.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/sdllog.hpp"
#include "editor/editor.hpp"
#include "editor/editorgame.hpp"
#include "editor/entitypanels.hpp"
#include "mazegame.hpp"
#include "save.hpp"
#include "uidemo.hpp"

using engine::application::Application;

namespace
{
	std::unique_ptr<engine::application::ScheduledGame> MakeEditedGame()
	{
		auto made = std::make_unique<game::MazeGame>();
		if (!made->IsValid()) return nullptr;
		// The editor makes several games and throws them away; none may write the player's save file.
		made->world.resources.Get<game::SaveFile>().path.clear();
		return made;
	}

	void ResolveAfterLoad(engine::ecs::World& world)
	{
		if (const auto* clips = world.resources.TryGet<game::SpriteClips>())
			game::ResolveClipNames(world.registry, *clips);
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
	                                         .sceneFolder = engine::core::ExecutableDirectory() / "assets" / "scenes"});
	if (!game.IsValid()) return 1;
	editor.SetVisible(true);
	app.SetEditorHost(&editor);
	return app.Run(game);
}
