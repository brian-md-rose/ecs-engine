#pragma once
#include <cstdint>

#include "assets.hpp"
#include "ecs/commandbuffer.hpp"
#include "ecs/registry.hpp"
#include "graphics/transform.hpp"
#include "tilemap.hpp"

// Turns a TileMap into the entities that draw it.
namespace game
{
	inline constexpr float kTileSize = 8.0f; // logical pixels per tile, both ways

	inline constexpr std::uint8_t kWallLayer = 0;
	inline constexpr std::uint8_t kPelletLayer = 1;

	// The top-left corner of a tile, in logical pixels, for a maze whose top-left tile is
	// drawn at origin.
	[[nodiscard]] engine::graphics::Position TileTopLeft(engine::graphics::Position origin, TilePosition tile);

	// Creates an entity with a Transform, a Sprite and a tag (Wall, Pellet, PowerPellet or
	// Door) for every such tile, with the maze's top-left tile at origin. Each wall gets the
	// wall tile that joins up with its neighbours (WallMask); power pellets also get an
	// Animation. Spawns, tunnels and empty tiles get no entity: the game places its actors from
	// the map. Creates entities directly, so call it during setup, not from a system.
	void SpawnMaze(engine::ecs::Registry& registry, const TileMap& map, engine::graphics::Position origin,
	               const SpriteClips& clips);

	// The pellets and power pellets of SpawnMaze alone, each also with an OnTile: creates the
	// entities and records their components in commands, so systems and event handlers can
	// put the pellets back when a level restarts.
	void SpawnPellets(engine::ecs::Registry& registry, engine::ecs::CommandBuffer& commands, const TileMap& map,
	                  engine::graphics::Position origin, const SpriteClips& clips);

	// The player, at the map's one player spawn, running at speed.
	void SpawnPlayer(engine::ecs::Registry& registry, const TileMap& map, engine::graphics::Position origin,
	                 const SpriteClips& clips, float speed);
} // namespace game
