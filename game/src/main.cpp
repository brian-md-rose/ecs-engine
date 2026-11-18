#include <SDL3/SDL_main.h> // provides WinMain when the game is a Windows GUI app (see game/CMakeLists.txt)

#include "application/application.hpp"
#include "mazegame.hpp"

using engine::application::Application;

int main()
{
	Application app(game::MazeGame::kConfig);
	game::MazeGame game;
	if (!game.IsValid()) return 1; // the maze couldn't be loaded; the error is on stderr
	return app.Run(game);
}
