#include "maze.hpp"

#include <cstddef>
#include <vector>

#include "components.hpp"
#include "core/assert.hpp"
#include "ecs/entity.hpp"
#include "gameflow.hpp"
#include "graphics/animation.hpp"
#include "graphics/sprite.hpp"
#include "gridmovement.hpp"
#include "looks.hpp"
#include "mazegame.hpp"
#include "walltiles.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::CommandBuffer;
		using engine::ecs::Entity;
		using engine::ecs::Registry;
		using engine::graphics::Animation;
		using engine::graphics::Clip;
		using engine::graphics::Position;
		using engine::graphics::Sprite;
		using engine::graphics::Transform;

		template <typename Tag> void SpawnTile(Registry& registry, Position position, const Sprite& sprite)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, position);
			registry.Add<Sprite>(entity, sprite);
			registry.Add<Tag>(entity);
			registry.Add<engine::ecs::DespawnOnEnter<Screen>>(entity, WorldScope());
		}

		// Created now, its components recorded in commands. Animated if its clip has frames to
		// step through.
		template <typename Tag>
		void SpawnEdible(Registry& registry, CommandBuffer& commands, TilePosition tile, Position position,
		                 const Clip& clip)
		{
			const Entity entity = registry.Create();
			commands.Add<Transform>(entity, position);
			commands.Add<Sprite>(entity, Sprite{.source = clip.firstFrame, .layer = kPelletLayer});
			if (clip.frames > 1) commands.Add<Animation>(entity, Animation{.clip = clip});
			commands.Add<Tag>(entity);
			commands.Add<OnTile>(entity, OnTile{.tile = tile});
			commands.Add<engine::ecs::DespawnOnEnter<Screen>>(entity, WorldScope());
		}
	} // namespace

	Position TileTopLeft(Position origin, TilePosition tile)
	{
		return {origin.x + static_cast<float>(tile.column) * kTileSize,
		        origin.y + static_cast<float>(tile.row) * kTileSize};
	}

	void SpawnMaze(Registry& registry, const TileMap& map, Position origin, const SpriteClips& clips)
	{
		for (std::size_t row = 0; row < map.Height(); ++row)
		{
			for (std::size_t column = 0; column < map.Width(); ++column)
			{
				const TilePosition tile{.column = column, .row = row};
				const Position at = TileTopLeft(origin, tile);
				switch (map.At(tile))
				{
				case Tile::Wall:
					SpawnTile<Wall>(registry, at,
					                Sprite{.source = clips.walls[WallMask(map, tile)].firstFrame, .layer = kWallLayer});
					break;
				case Tile::Door:
					SpawnTile<Door>(registry, at, Sprite{.source = clips.door.firstFrame, .layer = kPelletLayer});
					break;
				case Tile::Pellet:
				case Tile::PowerPellet: // SpawnPellets, below
				case Tile::Empty:
				case Tile::PlayerSpawn:
				case Tile::EnemySpawn:
				case Tile::Tunnel:
					break;
				}
			}
		}

		CommandBuffer commands;
		SpawnPellets(registry, commands, map, origin, clips);
		commands.Apply(registry);
	}

	void SpawnPellets(Registry& registry, CommandBuffer& commands, const TileMap& map, Position origin,
	                  const SpriteClips& clips)
	{
		for (const TilePosition& tile : map.Find(Tile::Pellet))
		{
			SpawnEdible<Pellet>(registry, commands, tile, TileTopLeft(origin, tile), clips.pellet);
		}
		for (const TilePosition& tile : map.Find(Tile::PowerPellet))
		{
			SpawnEdible<PowerPellet>(registry, commands, tile, TileTopLeft(origin, tile), clips.powerPellet);
		}
	}

	void SpawnPlayer(Registry& registry, const TileMap& map, Position origin, const SpriteClips& clips, float speed)
	{
		const std::vector<TilePosition> spawns = map.Find(Tile::PlayerSpawn);
		ENGINE_VERIFY(spawns.size() == 1, "the maze needs exactly one player spawn; ParseTileMap checks this");
		const Entity player = registry.Create();
		const GridMover mover{.tile = spawns.front(), .speed = speed};
		registry.Add<Transform>(player, MoverTopLeft(origin, mover));
		registry.Add<GridMover>(player, mover);
		registry.Add<engine::graphics::Interpolation>(player, kMoverInterpolation);
		registry.Add<Spawn>(player, Spawn{.tile = mover.tile});
		registry.Add<PlayerControlled>(player);
		registry.Add<Facing>(player);
		registry.Add<Sprite>(player, Sprite{.layer = kPlayerLayer});
		registry.Add<Animation>(player, Animation{.clip = clips.playerDown});
		registry.Add<engine::ecs::DespawnOnEnter<Screen>>(player, WorldScope());
	}
} // namespace game
