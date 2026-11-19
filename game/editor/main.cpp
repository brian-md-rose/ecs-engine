// tangle_run_editor: Tangle Run with the Dear ImGui editor overlay (F1). The reference integration of
// engine::editor, built only when ENGINE_BUILD_EDITOR is ON (docs/editor.md).
#include <SDL3/SDL_main.h>

#include "application/application.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/sdllog.hpp"
#include "editor/editor.hpp"
#include "editor/entitypanels.hpp"
#include "mazegame.hpp"
#include "save.hpp"

using engine::application::Application;

int main(int, char**)
{
	const std::filesystem::path logFolder =
	    engine::core::SaveDirectory(game::kSaveOrganisation, game::kSaveApplication);
	const engine::core::LogSession logging(
	    {.file = logFolder.empty() ? std::filesystem::path() : logFolder / "editor-log.txt"});
	engine::core::RouteSdlLog();

	Application app(game::MazeGame::kConfig);
	game::MazeGame game;
	if (!game.IsValid()) return 1;

	engine::editor::Editor editor;
	engine::editor::AddEntityPanels(editor, game.world);
	app.SetEditorHost(&editor);
	return app.Run(game);
}
