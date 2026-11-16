#include "maze.hpp"

#include <cstddef>

#include "components.hpp"
#include "ecs/entity.hpp"
#include "graphics/animation.hpp"
#include "graphics/sprite.hpp"
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
} // namespace game
