#include "application/application.hpp"
#include "mazegame.hpp"

int main()
{
	engine::application::Application app({.title = "ecs-engine",
	                                      .windowWidth = game::kLogicalWidth * game::kWindowScale,
	                                      .windowHeight = game::kLogicalHeight * game::kWindowScale,
	                                      .logicalWidth = game::kLogicalWidth,
	                                      .logicalHeight = game::kLogicalHeight});
	game::MazeGame game;
	return app.Run(game);
}
