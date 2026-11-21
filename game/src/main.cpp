#include <SDL3/SDL_main.h> // provides WinMain when the game is a Windows GUI app (see game/CMakeLists.txt)

#include "application/application.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/sdllog.hpp"
#include "graphics/screenshot.hpp"
#include "mazegame.hpp"
#include "save.hpp"

using engine::application::Application;

int main(int argc, char** argv) // SDL_main.h renames this to SDL_main and calls it with argc and argv
{
	// Next to the save file. The game is a GUI app in Release, where nobody sees the console.
	const std::filesystem::path logFolder =
	    engine::core::SaveDirectory(game::kSaveOrganisation, game::kSaveApplication);
	const engine::core::LogSession logging(
	    {.file = logFolder.empty() ? std::filesystem::path() : logFolder / "log.txt"});
	engine::core::RouteSdlLog(); // SDL's own messages go to the same file

	Application app(game::MazeGame::kConfig);
	app.SetScreenshotFolder(engine::graphics::ScreenshotDirectory(logFolder));
	app.SetScreenshotArgs(engine::graphics::ParseScreenshotArgs(argc, argv));
	game::MazeGame game;
	if (!game.IsValid()) return 1; // the maze couldn't be loaded; the error is in the log
	return app.Run(game);
}
