#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>

#include "components.hpp"
#include "difficulty.hpp"
#include "direction.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "enemymodes.hpp"
#include "enemysystems.hpp"
#include "enemytuning.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "graphics/color.hpp"
#include "graphics/shape.hpp"
#include "input/keyboard.hpp"
#include "mazegame.hpp"
#include "scoring.hpp"
#include "targeting.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::graphics::Color;
	using engine::input::Key;
	using game::EnemyBrain;
	using game::EnemyState;
	using game::Personality;
	using game::Stage;
	using game::TilePosition;

	constexpr double kStep = 1.0 / 60.0;
	constexpr int kStepsPerSecond = 60;

	[[nodiscard]] game::TileMap Parse(std::string_view text)
	{
		game::TileMapResult result = game::ParseTileMap(text);
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	[[nodiscard]] game::TileMap Level1()
	{
		const std::filesystem::path file = std::filesystem::path{GAME_ASSETS_DIR} / "mazes" / "level1.txt";
		game::TileMapResult result = game::LoadTileMap(file);
		REQUIRE(result.map.has_value());
		return *result.map;
	}

	// One corridor: the player at the left end, a pellet, a power pellet, and an enemy spawn at
	// the right end. No door, so the enemy (Grit) starts out in the maze. The pellet shut in
	// below can't be eaten, so the level never ends.
	[[nodiscard]] game::TileMap Corridor()
	{
		return Parse("##########\n"
		             "#P.o    E#\n"
		             "##########\n"
		             "#.########\n"
		             "##########\n");
	}

	// A power pellet next to the player, and the enemy shut in below where it can't be reached,
	// next to a pellet that can't be eaten either, so the level never ends.
	[[nodiscard]] game::TileMap ShutIn()
	{
		return Parse("#######\n"
		             "#Po   #\n"
		             "#######\n"
		             "#E#.###\n"
		             "#######\n");
	}

	[[nodiscard]] bool SameColour(Color a, Color b)
	{
		return a.r == b.r && a.g == b.g && a.b == b.b;
	}

	// The game as main.cpp runs it, minus the window (as in systems_tests.cpp).
	struct Harness
	{
		game::MazeGame maze;
		engine::input::Keyboard keyboard;

		explicit Harness(const game::TileMap& map) : maze(map)
		{
			maze.world.resources.Get<game::GameFlow>() = game::GameFlow{}; // past the title screen
			UpdateUntilStage(Stage::Playing);
		}

		[[nodiscard]] game::GameFlow& Flow() { return maze.world.resources.Get<game::GameFlow>(); }
		[[nodiscard]] game::ModeClock& Clock() { return maze.world.resources.Get<game::ModeClock>(); }
		[[nodiscard]] int Points() { return maze.world.resources.Get<game::Score>().points; }

		void Update(int steps = 1)
		{
			for (int i = 0; i < steps; ++i)
			{
				maze.Update(kStep, keyboard);
				keyboard.EndUpdate();
			}
		}

		void UpdateUntilStage(Stage stage)
		{
			for (int i = 0; i < 60 * kStepsPerSecond && Flow().stage != stage; ++i)
			{
				Update();
			}
			REQUIRE(Flow().stage == stage);
		}

		void Tap(Key key)
		{
			keyboard.OnKeyDown(key);
			Update();
			keyboard.OnKeyUp(key);
		}

		// The first enemy with this personality.
		[[nodiscard]] Entity EnemyOf(Personality personality)
		{
			std::optional<Entity> found;
			maze.world.registry.Each<game::EnemyPersonality>([&](Entity entity, const game::EnemyPersonality& who)
			{
				if (who.kind == personality && !found) found = entity;
			});
			REQUIRE(found.has_value());
			return *found;
		}

		[[nodiscard]] EnemyBrain& Brain(Entity enemy) { return maze.world.registry.GetComponent<EnemyBrain>(enemy); }
		[[nodiscard]] game::GridMover& Mover(Entity enemy)
		{
			return maze.world.registry.GetComponent<game::GridMover>(enemy);
		}
		[[nodiscard]] engine::graphics::Shape& Look(Entity enemy)
		{
			return maze.world.registry.GetComponent<engine::graphics::Shape>(enemy);
		}
	};
} // namespace

TEST_CASE("MazeGame puts an enemy on every spawn, with the personalities in turn", "[game][enemies]")
{
	Harness h(Level1());
	int enemies = 0;
	h.maze.world.registry.Each<game::Enemy, game::EnemyPersonality, game::Spawn>(
	    [&](Entity, const game::Enemy&, const game::EnemyPersonality& who, const game::Spawn& spawn)
	{
		CHECK(who.kind == game::PersonalityForSpawn(static_cast<std::size_t>(enemies)));
		CHECK(h.maze.world.resources.Get<game::TileMap>().At(spawn.tile) == game::Tile::EnemySpawn);
		++enemies;
	});
	CHECK(enemies == 4);
	CHECK(h.maze.world.resources.Get<game::EnemyHouse>().house.has_value());
}

TEST_CASE("Enemies leave the house one after another and come out of the door", "[game][enemies]")
{
	Harness h(Level1());
	const game::House house = *h.maze.world.resources.Get<game::EnemyHouse>().house;

	// Watch each one come out: when, and where.
	std::array<float, game::kPersonalityCount> outAt{};
	std::array<bool, game::kPersonalityCount> out{};
	float seconds = 0.0f;
	for (int step = 0; step < 12 * kStepsPerSecond && h.Flow().stage == Stage::Playing; ++step)
	{
		h.Update();
		seconds += static_cast<float>(kStep);
		for (std::size_t i = 0; i < game::kPersonalityCount; ++i)
		{
			const Entity enemy = h.EnemyOf(static_cast<Personality>(i));
			if (out[i] || h.Brain(enemy).state != EnemyState::Roaming) continue;
			out[i] = true;
			outAt[i] = seconds;
			CHECK(h.Mover(enemy).tile == house.exit);
			CHECK_FALSE(h.Mover(enemy).throughDoor);
		}
	}
	for (std::size_t i = 0; i < game::kPersonalityCount; ++i)
	{
		CAPTURE(i);
		REQUIRE(out[i]);
		CHECK(outAt[i] >= game::kPersonalities[i].releaseSeconds);
		if (i > 0) CHECK(outAt[i] > outAt[i - 1]);
	}
}

TEST_CASE("An enemy that isn't frightened catches the player, and the round restarts", "[game][enemies]")
{
	Harness h(Corridor());
	const Entity grit = h.EnemyOf(Personality::Grit);
	h.Update(); // out at once: there is no house
	CHECK(h.Brain(grit).state == EnemyState::Roaming);

	h.UpdateUntilStage(Stage::Dying); // the player stands still; Grit comes down the corridor
	CHECK(h.Flow().lives == game::kStartingLives - 1);

	h.UpdateUntilStage(Stage::Ready);
	CHECK(h.Mover(grit).tile == TilePosition{.column = 8, .row = 1});
	CHECK(h.Mover(grit).direction == game::Direction::None);
	CHECK(h.Brain(grit).state == EnemyState::InHouse);
	CHECK(h.Clock().phaseSeconds == 0.0f);
}

TEST_CASE("A power pellet frightens the enemies, and the player can eat one for points", "[game][enemies]")
{
	Harness h(Corridor());
	const Entity grit = h.EnemyOf(Personality::Grit);

	h.Tap(Key::Right);
	for (int i = 0; i < kStepsPerSecond && !game::IsFrightActive(h.Clock()); ++i)
	{
		h.Update();
	}
	REQUIRE(game::IsFrightActive(h.Clock()));
	h.Update(); // the enemies react on the next update
	CHECK(h.Brain(grit).frightened);
	CHECK(h.Mover(grit).direction == game::Direction::Right); // it was coming left: it turned back
	CHECK(h.Mover(grit).speed == game::FallbackDifficulty().ForLevel(1).frightenedSpeed);
	CHECK(SameColour(h.Look(grit).color, game::kFrightenedColor));

	const int beforeEating = h.Points();
	for (int i = 0; i < 3 * kStepsPerSecond && h.Brain(grit).state != EnemyState::Returning; ++i)
	{
		h.Update();
	}
	CHECK(h.Brain(grit).state == EnemyState::Returning);
	CHECK(h.Points() == beforeEating + game::EnemyPoints(0));
	CHECK(h.Clock().eatenInFright == 1);
	CHECK(h.Flow().stage == Stage::Playing); // eaten, not caught
	CHECK(h.Flow().lives == game::kStartingLives);
	h.Update(); // it changes its look on the next update
	CHECK(h.Look(grit).radius == game::kReturningLook.radius);
}

TEST_CASE("The fright flashes before it ends, then the enemies are dangerous again", "[game][enemies]")
{
	Harness h(ShutIn());
	const Entity grit = h.EnemyOf(Personality::Grit);
	h.Tap(Key::Right);
	for (int i = 0; i < kStepsPerSecond && !game::IsFrightActive(h.Clock()); ++i)
	{
		h.Update();
	}
	REQUIRE(h.Brain(grit).frightened);

	// Steps until only the warning is left, and a little into it.
	const float fright = game::FallbackDifficulty().ForLevel(1).frightSeconds;
	h.Update(static_cast<int>((fright - game::kFrightWarningSeconds) * kStepsPerSecond) + 2);
	REQUIRE(h.Clock().frightSeconds < game::kFrightWarningSeconds);
	bool flashed = false;
	bool drained = false;
	for (int i = 0; i < kStepsPerSecond; ++i)
	{
		h.Update();
		flashed = flashed || SameColour(h.Look(grit).color, game::kFrightFlashColor);
		drained = drained || SameColour(h.Look(grit).color, game::kFrightenedColor);
	}
	CHECK(flashed);
	CHECK(drained);

	h.Update(static_cast<int>(game::kFrightWarningSeconds * kStepsPerSecond));
	CHECK_FALSE(game::IsFrightActive(h.Clock()));
	CHECK_FALSE(h.Brain(grit).frightened);
	CHECK(game::IsDangerous(h.Brain(grit)));
	CHECK(SameColour(h.Look(grit).color, game::TuningFor(Personality::Grit).look.color));
}

TEST_CASE("Enemies wait while the round isn't being played", "[game][enemies]")
{
	Harness h(Level1());
	h.Update(kStepsPerSecond);
	const Entity grit = h.EnemyOf(Personality::Grit);
	const game::GridMover moving = h.Mover(grit);

	h.Tap(Key::P);
	h.Update(kStepsPerSecond);
	CHECK(h.Mover(grit).tile == moving.tile);
	CHECK(h.Mover(grit).offset == moving.offset);
}

TEST_CASE("EnemyLook drains a frightened enemy's colour and shrinks one going home", "[game][enemies]")
{
	const game::ModeClock calm;
	const engine::graphics::Shape grit = game::EnemyLook(Personality::Grit, EnemyBrain{}, calm);
	CHECK(SameColour(grit.color, game::TuningFor(Personality::Grit).look.color));
	CHECK(grit.layer == game::kEnemyLayer);

	const EnemyBrain frightened{.state = EnemyState::Roaming, .frightened = true};
	game::ModeClock clock{.frightSeconds = game::kFrightWarningSeconds + 1.0f};
	CHECK(SameColour(game::EnemyLook(Personality::Snare, frightened, clock).color, game::kFrightenedColor));
	CHECK(game::EnemyLook(Personality::Snare, frightened, clock).kind == engine::graphics::ShapeKind::Circle);

	// In the warning, a flash every 1 / kFrightFlashesPerSecond seconds: one half drained, one half lit.
	const float half = 0.5f / game::kFrightFlashesPerSecond;
	clock.frightSeconds = 2.0f * half + half / 2.0f;
	CHECK(SameColour(game::EnemyLook(Personality::Snare, frightened, clock).color, game::kFrightenedColor));
	clock.frightSeconds = 3.0f * half + half / 2.0f;
	CHECK(SameColour(game::EnemyLook(Personality::Snare, frightened, clock).color, game::kFrightFlashColor));

	const EnemyBrain returning{.state = EnemyState::Returning};
	CHECK(game::EnemyLook(Personality::Wisp, returning, calm).radius == game::kReturningLook.radius);
}
