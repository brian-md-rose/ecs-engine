#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include "components.hpp"
#include "ecs/entity.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "events.hpp"
#include "gameflow.hpp"
#include "input/input.hpp"
#include "mazegame.hpp"
#include "options.hpp"
#include "scoring.hpp"
#include "sounds.hpp"
#include "stage.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::FrameContext;
	using engine::ecs::World;
	using engine::input::Key;
	using game::Sfx;

	constexpr double kStep = 1.0 / 60.0;
} // namespace

TEST_CASE("Each event has its own sound", "[game][sounds]")
{
	STATIC_CHECK(game::SoundFor(game::PelletEaten{}) == Sfx::Eat);
	STATIC_CHECK(game::SoundFor(game::PowerPelletEaten{}) == Sfx::PowerUp);
	STATIC_CHECK(game::SoundFor(game::EnemyEaten{}) == Sfx::EnemyEaten);
	STATIC_CHECK(game::SoundFor(game::PlayerCaught{}) == Sfx::Death);
	STATIC_CHECK(game::SoundFor(game::LevelCleared{}) == Sfx::LevelClear);
}

TEST_CASE("Every sound is synthesised, short, and not too loud", "[game][sounds]")
{
	const game::SoundBank bank;
	for (std::size_t i = 0; i < game::kSfxCount; ++i)
	{
		CAPTURE(i);
		const engine::audio::Sound& sound = bank.Get(static_cast<Sfx>(i));
		CHECK_FALSE(sound.samples.empty());
		CHECK(sound.Seconds() < 1.5);
		for (const float sample : sound.samples)
		{
			REQUIRE(sample <= 0.5f);
			REQUIRE(sample >= -0.5f);
		}
	}
	// The pellet sound plays many times a second, so it must be over quickly.
	CHECK(bank.Get(Sfx::Eat).Seconds() < 0.1);
}

TEST_CASE("Queued sounds play once each update, without a device, and the queue empties", "[game][sounds]")
{
	World world;
	world.resources.Emplace<game::SoundBank>();
	world.resources.Emplace<game::SoundQueue>();
	world.resources.Emplace<game::Settings>();
	world.events.Subscribe<game::PelletEaten>(game::QueueSound<game::PelletEaten>);
	world.events.Subscribe<game::PlayerCaught>(game::QueueSound<game::PlayerCaught>);

	world.events.Publish(game::PelletEaten{});
	world.events.Publish(game::PlayerCaught{});
	world.events.Publish(game::PlayerCaught{}); // two enemies at once
	world.events.Deliver(world);

	const game::SoundQueue& queue = world.resources.Get<game::SoundQueue>();
	CHECK(queue.pending.size() == 3);

	game::AudioSystem(world, FrameContext{}); // no audio device: counted, not heard
	CHECK(queue.pending.empty());
	CHECK(queue.Played(Sfx::Eat) == 1);
	CHECK(queue.Played(Sfx::Death) == 1);
	CHECK(queue.Played(Sfx::PowerUp) == 0);
}

TEST_CASE("Playing the game makes the eating, power-up and level-clear sounds", "[game][sounds]")
{
	game::TileMapResult maze = game::ParseTileMap("#######\n"
	                                              "#P..o #\n"
	                                              "#######\n"
	                                              "#E#####\n"
	                                              "#######\n");
	REQUIRE(maze.map.has_value());
	game::MazeGame game{*maze.map};
	engine::input::Input input;
	const auto update = [&](int steps)
	{
		for (int i = 0; i < steps; ++i)
		{
			game.Update(kStep, input);
			input.EndUpdate();
		}
	};
	const game::SoundQueue& sounds = game.world.resources.Get<game::SoundQueue>();

	gametest::RequestPlaying(game);
	update(3 * 60); // past Ready
	REQUIRE(gametest::StageOf(game) == gametest::Stage::Playing);
	input.keyboard.OnKeyDown(Key::Right);
	update(2 * 60);

	CHECK(sounds.Played(Sfx::Eat) == 2);
	CHECK(sounds.Played(Sfx::PowerUp) == 1);
	CHECK(sounds.Played(Sfx::LevelClear) == 1);
	CHECK(sounds.Played(Sfx::Death) == 0);
}

TEST_CASE("With sound off in the options nothing is played, and the queue is still emptied", "[game][sounds]")
{
	game::TileMapResult maze = game::ParseTileMap("#######\n"
	                                              "#P..o #\n"
	                                              "#######\n"
	                                              "#E#####\n"
	                                              "#######\n");
	REQUIRE(maze.map.has_value());
	game::MazeGame game{*maze.map};
	game.world.resources.Get<game::Settings>().sound = false;
	engine::input::Input input;
	const auto update = [&](int steps)
	{
		for (int i = 0; i < steps; ++i)
		{
			game.Update(kStep, input);
			input.EndUpdate();
		}
	};

	gametest::RequestPlaying(game);
	update(3 * 60);
	input.keyboard.OnKeyDown(Key::Right);
	update(2 * 60);

	const game::SoundQueue& sounds = game.world.resources.Get<game::SoundQueue>();
	CHECK(game.world.resources.Get<game::Score>().points > 0); // the player did eat
	CHECK(sounds.Played(Sfx::Eat) == 0);
	CHECK(sounds.Played(Sfx::PowerUp) == 0);
	CHECK(sounds.Played(Sfx::LevelClear) == 0);
	CHECK(sounds.pending.empty());
}
