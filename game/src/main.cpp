#include "application/application.hpp"
#include "mazegame.hpp"

using engine::application::Application;

int main()
{
	Application app(game::MazeGame::kConfig);
	game::MazeGame game;
	return app.Run(game);
}
