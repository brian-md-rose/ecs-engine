#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <optional>

#include "assets.hpp"
#include "components.hpp"
#include "direction.hpp"
#include "ecs/entity.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "graphics/animation.hpp"
#include "graphics/sprite.hpp"
#include "input/keyboard.hpp"
#include "looks.hpp"
#include "mazegame.hpp"
#include "tilemap.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::graphics::Animation;
	using engine::graphics::Sprite;
	using engine::input::Key;
	using game::Direction;

	constexpr double kStep = 1.0 / 60.0;

	[[nodiscard]] const game::GameAssets& Assets()
	{
		static const std::optional<game::GameAssets> assets =
		    game::LoadGameAssets(std::filesystem::path{GAME_ASSETS_DIR});
		REQUIRE(assets.has_value());
		return *assets;
	}

	// The game with the real sprites, started and past the Ready pause.
	struct Harness
	{
		game::MazeGame maze;
		engine::input::Keyboard keyboard;

		Harness() : maze(Maze(), Assets())
		{
			maze.world.resources.Get<game::GameFlow>() = game::GameFlow{};
			for (int i = 0; i < 5 * 60 && Stage() != game::Stage::Playing; ++i)
			{
				Update();
			}
			REQUIRE(Stage() == game::Stage::Playing);
		}

		[[nodiscard]] static game::TileMap Maze()
		{
			game::TileMapResult result = game::ParseTileMap("#########\n"
			                                                "#...P...#\n"
			                                                "#.#####.#\n"
			                                                "#.......#\n"
			                                                "#########\n"
			                                                "#E#######\n"
			                                                "#########\n");
			REQUIRE(result.map.has_value());
			return *result.map;
		}

		[[nodiscard]] game::Stage Stage() const { return maze.world.resources.Get<game::GameFlow>().stage; }

		void Update(int steps = 1)
		{
			for (int i = 0; i < steps; ++i)
			{
				maze.Update(kStep, keyboard);
				keyboard.EndUpdate();
			}
		}

		void Tap(Key key)
		{
			keyboard.OnKeyDown(key);
			Update();
			keyboard.OnKeyUp(key);
			Update();
		}

		template <typename T> [[nodiscard]] T& PlayerComponent()
		{
			Entity player{};
			maze.world.registry.Each<game::PlayerControlled>([&player](Entity entity, const game::PlayerControlled&)
			{
				player = entity;
			});
			return maze.world.registry.GetComponent<T>(player);
		}
	};
} // namespace

TEST_CASE("The player faces the way it goes, mirrored for left, and has its own death", "[game][looks]")
{
	const game::SpriteClips& clips = Assets().clips;
	CHECK(game::PlayerPoseFor(clips, Direction::Right, false).clip == clips.playerRight);
	CHECK_FALSE(game::PlayerPoseFor(clips, Direction::Right, false).flipX);
	CHECK(game::PlayerPoseFor(clips, Direction::Left, false).clip == clips.playerRight);
	CHECK(game::PlayerPoseFor(clips, Direction::Left, false).flipX);
	CHECK(game::PlayerPoseFor(clips, Direction::Up, false).clip == clips.playerUp);
	CHECK(game::PlayerPoseFor(clips, Direction::Down, false).clip == clips.playerDown);
	CHECK(game::PlayerPoseFor(clips, Direction::Left, true).clip == clips.playerDeath);
	CHECK_FALSE(clips.playerDeath.loop);
}

TEST_CASE("Every sprite the game draws is loaded and animated where it should be", "[game][looks]")
{
	const game::SpriteClips& clips = Assets().clips;
	for (const engine::graphics::Clip& clip :
	     {clips.playerRight, clips.playerDown, clips.playerUp, clips.playerDeath, clips.frightened,
	      clips.frightenedFlash, clips.returning, clips.powerPellet})
	{
		CHECK(clip.firstFrame.width == 8);
		CHECK(clip.firstFrame.height == 8);
		CHECK(clip.frames > 1);
	}
	for (const engine::graphics::Clip& clip : clips.walls)
	{
		CHECK(clip.firstFrame.width == 8);
	}
	CHECK(clips.pellet.firstFrame.width == 8);
	CHECK(clips.door.firstFrame.width == 8);
}

TEST_CASE("The player walks with its animation and turns to face its way", "[game][looks]")
{
	Harness h;
	h.Tap(Key::Left);
	h.Update(10);
	CHECK(h.PlayerComponent<Animation>().clip == Assets().clips.playerRight);
	CHECK(h.PlayerComponent<Sprite>().flipX);

	// Frames step as it goes, and the sprite shows the frame.
	const engine::graphics::PixelRect before = h.PlayerComponent<Sprite>().source;
	h.Update(9); // over one frame's time (0.15 s)
	const engine::graphics::PixelRect after = h.PlayerComponent<Sprite>().source;
	CHECK_FALSE(before == after);
	CHECK(after == engine::graphics::AnimationSource(h.PlayerComponent<Animation>()));
}

TEST_CASE("Animations stand still while the game is paused", "[game][looks]")
{
	Harness h;
	h.Tap(Key::P);
	REQUIRE(h.maze.world.resources.Get<game::GameState>().paused);
	const float elapsed = h.PlayerComponent<Animation>().elapsed;
	h.Update(60);
	CHECK(h.PlayerComponent<Animation>().elapsed == elapsed);
}
