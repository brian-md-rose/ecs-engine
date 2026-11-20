// maze_to_scene: turns a maze text file (docs/maze-format.md) into a scene file of tile entities, for the editor
// (mazescene.hpp). Usage: maze_to_scene <maze.txt> <scene.json>. Built with ENGINE_BUILD_EDITOR.
#include <cstdio>
#include <filesystem>

#include "core/log.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/scene.hpp"
#include "ecs/typeregistry.hpp"
#include "mazegame.hpp"
#include "mazescene.hpp"
#include "scenes.hpp"
#include "tilemap.hpp"

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::fputs("usage: maze_to_scene <maze.txt> <scene.json>\n", stderr);
		return 2;
	}
	const game::TileMapResult loaded = game::LoadTileMap(argv[1]);
	if (!loaded.map)
	{
		std::fprintf(stderr, "%s: %s\n", argv[1], loaded.error.message.c_str());
		return 1;
	}

	engine::ecs::Registry registry;
	engine::ecs::TypeRegistry types;
	engine::ecs::RegisterEngineComponents(types);
	game::RegisterGameComponents(types);
	game::SpawnMazeScene(registry, *loaded.map, game::MazeOrigin(*loaded.map));

	engine::ecs::Issues issues;
	if (!engine::ecs::SaveSceneFile(registry, types, std::filesystem::path(argv[2]), issues))
	{
		for (const engine::ecs::Issue& issue : issues.errors)
			std::fprintf(stderr, "%s: %s\n", issue.path.c_str(), issue.message.c_str());
		return 1;
	}
	return 0;
}
