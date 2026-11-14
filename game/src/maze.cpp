#include "maze.hpp"

#include <cstddef>

#include "components.hpp"
#include "ecs/entity.hpp"
#include "graphics/shape.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::CommandBuffer;
		using engine::ecs::Entity;
		using engine::ecs::Registry;
		using engine::graphics::Position;
		using engine::graphics::Shape;
		using engine::graphics::ShapeKind;
		using engine::graphics::Transform;

		// A circle of this radius centred on the tile whose top-left corner is at tile.
		[[nodiscard]] Position CentredCircle(Position tile, float radius)
		{
			const float offset = kTileSize / 2.0f - radius;
			return {tile.x + offset, tile.y + offset};
		}

		template <typename Tag> void SpawnTile(Registry& registry, Position position, const Shape& shape)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, position);
			registry.Add<Shape>(entity, shape);
			registry.Add<Tag>(entity);
		}

		// Created now, its components recorded in commands.
		template <typename Tag>
		void SpawnEdible(Registry& registry, CommandBuffer& commands, TilePosition tile, Position position,
		                 const Shape& shape)
		{
			const Entity entity = registry.Create();
			commands.Add<Transform>(entity, position);
			commands.Add<Shape>(entity, shape);
			commands.Add<Tag>(entity);
			commands.Add<OnTile>(entity, OnTile{.tile = tile});
		}
	} // namespace

	Position TileTopLeft(Position origin, TilePosition tile)
	{
		return {origin.x + static_cast<float>(tile.column) * kTileSize,
		        origin.y + static_cast<float>(tile.row) * kTileSize};
	}

	void SpawnMaze(Registry& registry, const TileMap& map, Position origin)
	{
		const Shape wall{.width = kTileSize, .height = kTileSize, .color = kWallColor, .layer = kWallLayer};
		const Shape door{.width = kTileSize, .height = kDoorThickness, .color = kDoorColor, .layer = kPelletLayer};

		for (std::size_t row = 0; row < map.Height(); ++row)
		{
			for (std::size_t column = 0; column < map.Width(); ++column)
			{
				const Position tile = TileTopLeft(origin, {.column = column, .row = row});
				switch (map.At({.column = column, .row = row}))
				{
				case Tile::Wall:
					SpawnTile<Wall>(registry, tile, wall);
					break;
				case Tile::Door:
					SpawnTile<Door>(registry, {tile.x, tile.y + (kTileSize - kDoorThickness) / 2.0f}, door);
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
		SpawnPellets(registry, commands, map, origin);
		commands.Apply(registry);
	}

	void SpawnPellets(Registry& registry, CommandBuffer& commands, const TileMap& map, Position origin)
	{
		const Shape pellet{
		    .kind = ShapeKind::Circle, .radius = kPelletRadius, .color = kPelletColor, .layer = kPelletLayer};
		const Shape powerPellet{
		    .kind = ShapeKind::Circle, .radius = kPowerPelletRadius, .color = kPelletColor, .layer = kPelletLayer};

		for (const TilePosition& tile : map.Find(Tile::Pellet))
		{
			SpawnEdible<Pellet>(registry, commands, tile, CentredCircle(TileTopLeft(origin, tile), kPelletRadius),
			                    pellet);
		}
		for (const TilePosition& tile : map.Find(Tile::PowerPellet))
		{
			SpawnEdible<PowerPellet>(registry, commands, tile,
			                         CentredCircle(TileTopLeft(origin, tile), kPowerPelletRadius), powerPellet);
		}
	}
} // namespace game
