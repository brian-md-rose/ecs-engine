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
