#pragma once
#include <cstdint>

#include "ecs/commandbuffer.hpp"
#include "ecs/registry.hpp"
#include "graphics/color.hpp"
#include "graphics/transform.hpp"
#include "tilemap.hpp"

// Turns a TileMap into the entities that draw it.
namespace game
{
	inline constexpr float kTileSize = 8.0f; // logical pixels per tile, both ways
	inline constexpr float kPelletRadius = 1.0f;
	inline constexpr float kPowerPelletRadius = 3.0f;
	inline constexpr float kDoorThickness = 2.0f;

	inline constexpr std::uint8_t kWallLayer = 0;
	inline constexpr std::uint8_t kPelletLayer = 1;

	inline constexpr engine::graphics::Color kWallColor{30, 90, 170};
	inline constexpr engine::graphics::Color kPelletColor{240, 220, 190};
	inline constexpr engine::graphics::Color kDoorColor{230, 140, 190};

	// The top-left corner of a tile, in logical pixels, for a maze whose top-left tile is
	// drawn at origin.
	[[nodiscard]] engine::graphics::Position TileTopLeft(engine::graphics::Position origin, TilePosition tile);

	// Creates an entity with a Transform, a Shape and a tag (Wall, Pellet, PowerPellet or
	// Door) for every such tile, with the maze's top-left tile at origin. Pellets are centred
	// on their tile; the door is a thin bar across the middle of its tile. Spawns, tunnels and
	// empty tiles get no entity: the game places its actors from the map.
	// Creates entities directly, so call it during setup, not from a system.
	void SpawnMaze(engine::ecs::Registry& registry, const TileMap& map, engine::graphics::Position origin);

	// The pellets and power pellets of SpawnMaze alone, each also with an OnTile: creates the
	// entities and records their components in commands, so systems and event handlers can
	// put the pellets back when a level restarts.
	void SpawnPellets(engine::ecs::Registry& registry, engine::ecs::CommandBuffer& commands, const TileMap& map,
	                  engine::graphics::Position origin);
} // namespace game
