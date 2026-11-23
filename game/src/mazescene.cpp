#include "mazescene.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "components.hpp"
#include "core/log.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/name.hpp"
#include "ecs/scene.hpp"
#include "ecs/states.hpp"
#include "ecs/typeregistry.hpp"
#include "ecs/world.hpp"
#include "enemysystems.hpp"
#include "gameflow.hpp"
#include "graphics/animation.hpp"
#include "graphics/sprite.hpp"
#include "maze.hpp"
#include "mazegame.hpp"
#include "scenes.hpp"
#include "screens.hpp"
#include "targeting.hpp"
#include "walltiles.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::Entity;
		using engine::ecs::Registry;
		using engine::graphics::Position;
		using engine::graphics::Sprite;
		using engine::graphics::Transform;

		TileMapResult Fail(std::size_t line, std::size_t column, std::string message)
		{
			return {.map = std::nullopt, .error = {.line = line, .column = column, .message = std::move(message)}};
		}

		// The one MazeLayout, or null (with the reason in message) if there are none or several.
		const MazeLayout* FindLayout(Registry& registry, std::string& message)
		{
			const MazeLayout* found = nullptr;
			std::size_t count = 0;
			registry.Each<MazeLayout>([&](Entity, const MazeLayout& layout)
			{
				found = &layout;
				++count;
			});
			if (count == 0) message = "the scene has no MazeLayout";
			if (count > 1) message = "the scene has more than one MazeLayout";
			return count == 1 ? found : nullptr;
		}

		// The character the maze format uses for the tile entity's tag, or 0 if it has none or several.
		char TileCharacter(const Registry& registry, Entity entity)
		{
			char found = 0;
			int count = 0;
			const auto note = [&](bool has, char character)
			{
				if (!has) return;
				found = character;
				++count;
			};
			note(registry.HasComponent<Wall>(entity), '#');
			note(registry.HasComponent<Door>(entity), '-');
			note(registry.HasComponent<Pellet>(entity), '.');
			note(registry.HasComponent<PowerPellet>(entity), 'o');
			note(registry.HasComponent<PlayerStart>(entity), 'P');
			note(registry.HasComponent<EnemyStart>(entity), 'E');
			note(registry.HasComponent<TunnelTile>(entity), 'T');
			return count == 1 ? found : char{0};
		}

		// The tiles as they stand, without the maze format's checks: a tile outside the layout is skipped, and of two
		// on one cell the later one counts. Enough to draw the maze, and to join its walls, while it is half edited.
		TileMap LenientTileMap(Registry& registry, const MazeLayout& layout)
		{
			const auto columns = static_cast<std::size_t>(std::max(layout.columns, 0));
			const auto rows = static_cast<std::size_t>(std::max(layout.rows, 0));
			std::vector<Tile> tiles(columns * rows, Tile::Empty);
			registry.Each<TileCell>([&](Entity entity, const TileCell& cell)
			{
				if (cell.column < 0 || cell.row < 0 || cell.column >= layout.columns || cell.row >= layout.rows) return;
				Tile tile = Tile::Empty;
				switch (TileCharacter(registry, entity))
				{
				case '#':
					tile = Tile::Wall;
					break;
				case '-':
					tile = Tile::Door;
					break;
				case '.':
					tile = Tile::Pellet;
					break;
				case 'o':
					tile = Tile::PowerPellet;
					break;
				case 'P':
					tile = Tile::PlayerSpawn;
					break;
				case 'E':
					tile = Tile::EnemySpawn;
					break;
				case 'T':
					tile = Tile::Tunnel;
					break;
				default:
					return;
				}
				tiles[static_cast<std::size_t>(cell.row) * columns + static_cast<std::size_t>(cell.column)] = tile;
			});
			return TileMap(columns, rows, std::move(tiles));
		}

		std::string Where(const TileCell& cell)
		{
			return "line " + std::to_string(cell.row + 1) + ", column " + std::to_string(cell.column + 1);
		}

		template <typename Tag> void SpawnTileEntity(Registry& registry, const char* name, int column, int row)
		{
			const Entity entity = registry.Create();
			registry.Add<engine::ecs::Name>(entity, engine::ecs::Name{.value = name});
			registry.Add<TileCell>(entity, TileCell{.column = column, .row = row});
			registry.Add<Tag>(entity);
		}
	} // namespace

	void RegisterMazeComponents(engine::ecs::TypeRegistry& types)
	{
		types.Register<MazeLayout>("MazeLayout")
		    .Field("columns", &MazeLayout::columns)
		    .Field("rows", &MazeLayout::rows)
		    .Field("x", &MazeLayout::x)
		    .Field("y", &MazeLayout::y);
		types.Register<TileCell>("TileCell").Field("column", &TileCell::column).Field("row", &TileCell::row);
		types.RegisterTag<Wall>("Wall");
		types.RegisterTag<Door>("Door");
		types.RegisterTag<Pellet>("Pellet");
		types.RegisterTag<PowerPellet>("PowerPellet");
		types.RegisterTag<PlayerStart>("PlayerStart");
		types.RegisterTag<EnemyStart>("EnemyStart");
		types.RegisterTag<TunnelTile>("TunnelTile");
	}

	TileMapResult BuildTileMap(Registry& registry)
	{
		std::string message;
		const MazeLayout* layout = FindLayout(registry, message);
		if (layout == nullptr) return Fail(0, 0, message);
		if (layout->columns <= 0 || layout->rows <= 0) return Fail(0, 0, "the MazeLayout is empty");

		const auto columns = static_cast<std::size_t>(layout->columns);
		const auto rows = static_cast<std::size_t>(layout->rows);
		std::vector<std::string> grid(rows, std::string(columns, ' '));

		TileMapResult failure;
		bool failed = false;
		registry.Each<TileCell>([&](Entity entity, const TileCell& cell)
		{
			if (failed) return;
			if (cell.column < 0 || cell.row < 0 || cell.column >= layout->columns || cell.row >= layout->rows)
			{
				failure = Fail(0, 0,
				               "a tile at column " + std::to_string(cell.column + 1) + ", row " +
				                   std::to_string(cell.row + 1) + " is outside the maze (" +
				                   std::to_string(layout->columns) + " by " + std::to_string(layout->rows) + ")");
				failed = true;
				return;
			}
			const std::size_t row = static_cast<std::size_t>(cell.row);
			const std::size_t column = static_cast<std::size_t>(cell.column);
			const char character = TileCharacter(registry, entity);
			if (character == 0)
			{
				failure = Fail(row + 1, column + 1,
				               "the tile entity must have exactly one of Wall, Door, Pellet, PowerPellet, PlayerStart, "
				               "EnemyStart or TunnelTile");
				failed = true;
				return;
			}
			if (grid[row][column] != ' ')
			{
				failure = Fail(row + 1, column + 1, "two tiles on the same cell (" + Where(cell) + ")");
				failed = true;
				return;
			}
			grid[row][column] = character;
		});
		if (failed) return failure;

		std::string text;
		for (const std::string& line : grid)
		{
			text += line;
			text += '\n';
		}
		TileMapResult result = ParseTileMap(text);
		if (result.map && (result.map->Width() != columns || result.map->Height() != rows))
		{
			// A last row with nothing in it is dropped by the text format; the layout says it is there.
			return Fail(0, 0, "the maze's last row has no tiles; make the MazeLayout smaller or add a tile to it");
		}
		return result;
	}

	void SpawnMazeScene(Registry& registry, const TileMap& map, Position origin)
	{
		const Entity layout = registry.Create();
		registry.Add<engine::ecs::Name>(layout, engine::ecs::Name{.value = "Maze"});
		registry.Add<MazeLayout>(layout, MazeLayout{.columns = static_cast<int>(map.Width()),
		                                            .rows = static_cast<int>(map.Height()),
		                                            .x = origin.x,
		                                            .y = origin.y});
		for (std::size_t row = 0; row < map.Height(); ++row)
		{
			for (std::size_t column = 0; column < map.Width(); ++column)
			{
				const auto c = static_cast<int>(column);
				const auto r = static_cast<int>(row);
				switch (map.At({.column = column, .row = row}))
				{
				case Tile::Wall:
					SpawnTileEntity<Wall>(registry, "Wall", c, r);
					break;
				case Tile::Door:
					SpawnTileEntity<Door>(registry, "Door", c, r);
					break;
				case Tile::Pellet:
					SpawnTileEntity<Pellet>(registry, "Pellet", c, r);
					break;
				case Tile::PowerPellet:
					SpawnTileEntity<PowerPellet>(registry, "Power pellet", c, r);
					break;
				case Tile::PlayerSpawn:
					SpawnTileEntity<PlayerStart>(registry, "Player start", c, r);
					break;
				case Tile::EnemySpawn:
					SpawnTileEntity<EnemyStart>(registry, "Enemy start", c, r);
					break;
				case Tile::Tunnel:
					SpawnTileEntity<TunnelTile>(registry, "Tunnel", c, r);
					break;
				case Tile::Empty:
					break;
				}
			}
		}
		PlaceMazeTiles(registry);
	}

	bool PlaceMazeTiles(Registry& registry)
	{
		std::string message;
		const MazeLayout* layout = FindLayout(registry, message);
		if (layout == nullptr) return false;
		const Position origin{layout->x, layout->y};
		// Collected first: adding components is not allowed while Each is running.
		std::vector<std::pair<Entity, Position>> places;
		registry.Each<TileCell>([&](Entity entity, const TileCell& cell)
		{
			places.emplace_back(entity, TileTopLeft(origin, {.column = static_cast<std::size_t>(cell.column),
			                                                 .row = static_cast<std::size_t>(cell.row)}));
		});
		for (const auto& [entity, at] : places)
		{
			if (Transform* transform = registry.TryGetComponent<Transform>(entity))
				transform->position = at;
			else
				registry.Add<Transform>(entity, Transform{.position = at});
		}
		return true;
	}

	bool ResolveMazeTiles(Registry& registry, const SpriteClips& clips)
	{
		std::string message;
		const MazeLayout* layout = FindLayout(registry, message);
		if (layout == nullptr) return false;
		const TileMap map = LenientTileMap(registry, *layout);

		// Collected first: adding components is not allowed while Each is running.
		struct Tiled
		{
			Entity entity;
			TileCell cell;
			char kind;
		};
		std::vector<Tiled> tiles;
		registry.Each<TileCell>([&](Entity entity, const TileCell& cell)
		{
			tiles.push_back({entity, cell, TileCharacter(registry, entity)});
		});

		for (const Tiled& tile : tiles)
		{
			const TilePosition at{.column = static_cast<std::size_t>(tile.cell.column),
			                      .row = static_cast<std::size_t>(tile.cell.row)};
			Sprite sprite;
			switch (tile.kind)
			{
			case '#':
				sprite = Sprite{.source = clips.walls[WallMask(map, at)].firstFrame, .layer = kWallLayer};
				break;
			case '-':
				sprite = Sprite{.source = clips.door.firstFrame, .layer = kPelletLayer};
				break;
			case '.':
				sprite = Sprite{.source = clips.pellet.firstFrame, .layer = kPelletLayer};
				break;
			case 'o':
				sprite = Sprite{.source = clips.powerPellet.firstFrame, .layer = kPelletLayer};
				break;
			default:
				continue; // spawns and tunnels have no picture
			}
			if (Sprite* existing = registry.TryGetComponent<Sprite>(tile.entity))
				*existing = sprite;
			else
				registry.Add<Sprite>(tile.entity, sprite);

			if (tile.kind == '.' || tile.kind == 'o')
			{
				if (!registry.HasComponent<OnTile>(tile.entity)) registry.Add<OnTile>(tile.entity, OnTile{.tile = at});
				if (tile.kind == 'o' && clips.powerPellet.frames > 1 &&
				    !registry.HasComponent<engine::graphics::Animation>(tile.entity))
					registry.Add<engine::graphics::Animation>(tile.entity,
					                                          engine::graphics::Animation{.clip = clips.powerPellet});
			}
		}
		return true;
	}

	std::string AdoptSceneMaze(engine::ecs::World& world)
	{
		TileMapResult built = BuildTileMap(world.registry);
		if (!built.map)
		{
			std::string problem = built.error.message;
			if (built.error.line > 0)
				problem += " (line " + std::to_string(built.error.line) + ", column " +
				           std::to_string(built.error.column) + ")";
			engine::core::LogError("maze scene: " + problem);
			return problem;
		}
		const auto* clips = world.resources.TryGet<SpriteClips>();
		if (clips == nullptr || !ResolveMazeTiles(world.registry, *clips))
			return "the game has no pictures to draw the maze with";

		const TileMap& map = world.resources.Get<TileMap>() = std::move(*built.map);
		world.resources.Get<EnemyHouse>().house = FindHouse(map);
		// Not reset on returning to the Title: SpawnGameWorld checks that the scene's tiles still exist, and a later
		// game in this world then spawns tiles from the (scene's) TileMap in code. Only the editor's throwaway play
		// world gets here.
		world.resources.Get<GameWorld>().fromScene = true;
		// Starts the game on its own maze: entering Playing spawns the player, enemies and HUD but not the maze.
		world.resources.Get<engine::ecs::NextState<Screen>>().Set(Screen::Playing);
		return {};
	}

	std::string PlacePlayerStart(engine::ecs::World& world, int column, int row)
	{
		const TileMap& map = world.resources.Get<TileMap>();
		if (column < 0 || row < 0 || !map.Contains({static_cast<std::size_t>(column), static_cast<std::size_t>(row)}))
			return "that cell is outside the maze";
		const TilePosition target{static_cast<std::size_t>(column), static_cast<std::size_t>(row)};
		switch (map.At(target))
		{
		case Tile::Wall:
			return "that cell is a wall";
		case Tile::Door:
			return "that cell is a door";
		case Tile::EnemySpawn:
			return "that cell is an enemy start";
		case Tile::Tunnel:
			return "that cell is a tunnel";
		default:
			break;
		}
		std::vector<Tile> grid;
		grid.reserve(map.Width() * map.Height());
		for (std::size_t y = 0; y < map.Height(); ++y)
		{
			for (std::size_t x = 0; x < map.Width(); ++x)
			{
				const TilePosition at{x, y};
				Tile tile = map.At(at);
				if (tile == Tile::PlayerSpawn) tile = Tile::Empty;
				if (at == target) tile = Tile::PlayerSpawn;
				grid.push_back(tile);
			}
		}
		world.resources.Get<TileMap>() = TileMap(map.Width(), map.Height(), std::move(grid));
		return {};
	}

	const std::vector<std::string>& MazePaletteNames()
	{
		static const std::vector<std::string> names{"Wall",         "Door",        "Pellet", "Power pellet",
		                                            "Player start", "Enemy start", "Tunnel"};
		return names;
	}

	void EraseMazeTile(Registry& registry, int column, int row)
	{
		std::vector<Entity> found;
		registry.Each<TileCell>([&](Entity entity, const TileCell& cell)
		{
			if (cell.column == column && cell.row == row) found.push_back(entity);
		});
		for (const Entity entity : found)
			registry.Destroy(entity);
	}

	void PaintMazeTile(Registry& registry, std::size_t kind, int column, int row)
	{
		std::string message;
		const MazeLayout* layout = FindLayout(registry, message);
		if (layout == nullptr || kind >= MazePaletteNames().size()) return;
		if (column < 0 || row < 0 || column >= layout->columns || row >= layout->rows) return;

		// Already exactly this tile: nothing to do, so a stroke over it doesn't churn the scene.
		constexpr char kCharacters[] = {'#', '-', '.', 'o', 'P', 'E', 'T'};
		const char wanted = kCharacters[kind];
		std::size_t there = 0;
		bool same = false;
		registry.Each<TileCell>([&](Entity entity, const TileCell& cell)
		{
			if (cell.column != column || cell.row != row) return;
			++there;
			same = TileCharacter(registry, entity) == wanted;
		});
		if (there == 1 && same) return;

		EraseMazeTile(registry, column, row);
		if (wanted == 'P')
		{
			std::vector<Entity> others;
			registry.Each<PlayerStart>([&](Entity entity, const PlayerStart&)
			{
				others.push_back(entity);
			});
			for (const Entity entity : others)
				registry.Destroy(entity);
		}
		switch (wanted)
		{
		case '#':
			SpawnTileEntity<Wall>(registry, "Wall", column, row);
			break;
		case '-':
			SpawnTileEntity<Door>(registry, "Door", column, row);
			break;
		case '.':
			SpawnTileEntity<Pellet>(registry, "Pellet", column, row);
			break;
		case 'o':
			SpawnTileEntity<PowerPellet>(registry, "Power pellet", column, row);
			break;
		case 'P':
			SpawnTileEntity<PlayerStart>(registry, "Player start", column, row);
			break;
		case 'E':
			SpawnTileEntity<EnemyStart>(registry, "Enemy start", column, row);
			break;
		default:
			SpawnTileEntity<TunnelTile>(registry, "Tunnel", column, row);
			break;
		}
	}

	bool HasSceneMaze(Registry& registry)
	{
		std::string message;
		return FindLayout(registry, message) != nullptr;
	}

	void ScopeMazeTiles(engine::ecs::Registry& registry)
	{
		std::vector<Entity> tiles;
		registry.Each<TileCell>([&](Entity entity, const TileCell&)
		{
			tiles.push_back(entity);
		});
		registry.Each<MazeLayout>([&](Entity entity, const MazeLayout&)
		{
			tiles.push_back(entity);
		});
		for (const Entity entity : tiles)
			registry.Add<engine::ecs::DespawnOnEnter<Screen>>(entity, WorldScope());
	}
} // namespace game

namespace game
{
	TileMapResult LoadMazeScene(const std::filesystem::path& file)
	{
		engine::ecs::TypeRegistry types;
		engine::ecs::RegisterEngineComponents(types);
		RegisterGameComponents(types);
		engine::ecs::Registry registry;
		const engine::ecs::SceneLoadResult loaded = engine::ecs::LoadSceneFile(registry, types, file);
		if (!loaded.Ok())
		{
			const engine::ecs::Issue& first = loaded.issues.errors.front();
			return {.map = std::nullopt,
			        .error = {.line = 0, .column = 0, .message = first.path + ": " + first.message}};
		}
		return BuildTileMap(registry);
	}
} // namespace game
