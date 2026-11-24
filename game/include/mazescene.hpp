#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "assets.hpp"
#include "ecs/registry.hpp"
#include "ecs/typeregistry.hpp"
#include "ecs/world.hpp"
#include "graphics/transform.hpp"
#include "tilemap.hpp"

// The maze as scene entities, so the editor can show and change it. A maze scene has one entity with a MazeLayout
// (how big the maze is and where its top-left tile is drawn) and one entity per tile that is not empty floor: a
// TileCell for the place, and one tag for the kind (Wall, Door, Pellet, PowerPellet, PlayerStart, EnemyStart or
// TunnelTile). Gameplay still reads the TileMap, which BuildTileMap works out from these entities, so the rules
// of docs/maze-format.md are checked in one place. A tile entity's Transform is where it is drawn and is worked
// out from its TileCell and the layout (PlaceMazeTiles); a wall's sprite is worked out from its neighbours
// (ResolveMazeTiles).
namespace game
{
	// Component: the maze's size in tiles and where its top-left tile is drawn, in logical pixels. One per scene.
	struct MazeLayout
	{
		int columns = 0;
		int rows = 0;
		float x = 0;
		float y = 0;
	};

	// Component: the tile an entity stands for, counted from the top-left at column 0, row 0.
	struct TileCell
	{
		int column = 0;
		int row = 0;
	};

	// Tags for the tiles that are not already Wall, Door, Pellet and PowerPellet (components.hpp).
	struct PlayerStart
	{
	};

	struct EnemyStart
	{
	};

	struct TunnelTile
	{
	};

	// Describes MazeLayout, TileCell and the tile tags (including Wall, Door, Pellet and PowerPellet) to a type
	// registry. RegisterGameComponents calls it.
	void RegisterMazeComponents(engine::ecs::TypeRegistry& types);

	// The TileMap the scene's tile entities describe, checked by ParseTileMap's rules. Without a map, error says what
	// is wrong: no MazeLayout or more than one, a TileCell outside the layout, two tiles on one cell, an entity with
	// more than one tile tag or none, or any of the maze format's own errors (line and column are the tile's, counted
	// from 1).
	[[nodiscard]] TileMapResult BuildTileMap(engine::ecs::Registry& registry);

	// Reads a maze scene file and builds its TileMap: LoadSceneFile into a scratch registry, then BuildTileMap. This is
	// how the game gets its maze. A scene that will not load gives no map, with the first problem as the message.
	[[nodiscard]] TileMapResult LoadMazeScene(const std::filesystem::path& file);

	// Makes the scene entities for a map: a MazeLayout at origin, and a Name, TileCell, tag and Transform for every
	// tile that is not empty floor. Creates entities directly, so call it during setup, not from a system.
	void SpawnMazeScene(engine::ecs::Registry& registry, const TileMap& map, engine::graphics::Position origin);

	// Sets every tile entity's Transform from its TileCell and the layout. False if there is no single layout.
	bool PlaceMazeTiles(engine::ecs::Registry& registry);

	// Gives the tile entities their pictures and what the game adds at run time: walls get the sprite that joins up
	// with their neighbours, doors, pellets and power pellets their sprite (power pellets also an Animation), and
	// pellets and power pellets an OnTile. Works on a maze that is not valid yet (as while it is being edited): a
	// tile outside the layout is left alone. False only if there is not exactly one MazeLayout.
	bool ResolveMazeTiles(engine::ecs::Registry& registry, const SpriteClips& clips);

	// Starts a game on the maze a level scene has put in the world, for the editor's Play: builds the TileMap from
	// the tile entities (replacing the game's own), resolves their pictures, tells SpawnGameWorld not to spawn
	// another maze, and asks for the Playing screen. Changes nothing but the log if the tiles are not a
	// valid maze. The world must be a MazeGame's, with its entities loaded. Returns what is wrong, or an empty string.
	[[nodiscard]] std::string AdoptSceneMaze(engine::ecs::World& world);

	// For the editor's Play from here: after AdoptSceneMaze, moves the player's start to the cell (column, row), so the
	// game starts the player there. The old start becomes an empty tile. Returns what is wrong (a wall, a door, an
	// enemy start, a cell outside the maze), or an empty string.
	[[nodiscard]] std::string PlacePlayerStart(engine::ecs::World& world, int column, int row);

	// What the editor can paint into a maze, in palette order: Wall, Door, Pellet, Power pellet, Player start, Enemy
	// start and Tunnel.
	[[nodiscard]] const std::vector<std::string>& MazePaletteNames();

	// Puts the tile of palette entry kind into the cell, replacing whatever tile is there; painting what is already
	// there changes nothing. A maze has one player start, so painting one moves it. Does nothing for a kind outside
	// the palette or a cell outside the layout. Call PlaceMazeTiles and ResolveMazeTiles afterwards: the new tile has
	// no Transform or picture yet.
	void PaintMazeTile(engine::ecs::Registry& registry, std::size_t kind, int column, int row);

	// The palette entry (an index into MazePaletteNames) of the tile in the cell, or nothing if the cell is empty or
	// outside the layout.
	[[nodiscard]] std::optional<std::size_t> MazeTileKindAt(engine::ecs::Registry& registry, int column, int row);

	// Removes the tile in the cell, if any.
	void EraseMazeTile(engine::ecs::Registry& registry, int column, int row);

	// True if the registry holds a maze scene's MazeLayout.
	[[nodiscard]] bool HasSceneMaze(engine::ecs::Registry& registry);

	// Makes the scene's tile entities go away on entering the Title, like the rest of the game's world. SpawnGameWorld
	// calls it when the maze came from a scene.
	void ScopeMazeTiles(engine::ecs::Registry& registry);
} // namespace game
