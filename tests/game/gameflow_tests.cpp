#include <catch2/catch_test_macros.hpp>

#include <initializer_list>

#include "gameflow.hpp"

namespace
{
	using game::GameFlow;
	using game::Stage;

	[[nodiscard]] GameFlow In(Stage stage, int lives = game::kStartingLives, int level = 1)
	{
		return GameFlow{.stage = stage, .stageSeconds = 0.0f, .lives = lives, .level = level};
	}

	// Ticks in fixed steps until at least seconds have passed, the way the game does.
	[[nodiscard]] GameFlow TickFor(GameFlow flow, float seconds)
	{
		constexpr float kStep = 1.0f / 60.0f;
		for (float elapsed = 0.0f; elapsed < seconds; elapsed += kStep)
		{
			flow = game::Tick(flow, kStep);
		}
		return flow;
	}
} // namespace

TEST_CASE("A game starts Ready on level 1 with every life", "[game][gameflow]")
{
	const GameFlow flow;
	CHECK(flow.stage == Stage::Ready);
	CHECK(flow.stageSeconds == 0.0f);
	CHECK(flow.lives == game::kStartingLives);
	CHECK(flow.level == 1);
}

TEST_CASE("Ready turns into Playing after its pause", "[game][gameflow]")
{
	GameFlow flow = game::Tick(In(Stage::Ready), game::kReadySeconds / 2.0f);
	CHECK(flow.stage == Stage::Ready);
	CHECK(flow.stageSeconds == game::kReadySeconds / 2.0f);

	flow = game::Tick(flow, game::kReadySeconds / 2.0f + 0.01f);
	CHECK(flow.stage == Stage::Playing);
	CHECK(flow.stageSeconds == 0.0f); // a new stage starts its clock at 0

	CHECK(TickFor(In(Stage::Ready), game::kReadySeconds).stage == Stage::Playing);
}

TEST_CASE("Playing and GameOver have no timer", "[game][gameflow]")
{
	CHECK(TickFor(In(Stage::Playing), 60.0f).stage == Stage::Playing);
	CHECK(TickFor(In(Stage::GameOver, 0), 60.0f).stage == Stage::GameOver);
}

TEST_CASE("Being caught while playing costs a life", "[game][gameflow]")
{
	GameFlow flow = game::Tick(In(Stage::Playing), 3.0f);
	flow = game::CatchPlayer(flow);
	CHECK(flow.stage == Stage::Dying);
	CHECK(flow.lives == game::kStartingLives - 1);
	CHECK(flow.stageSeconds == 0.0f);

	flow = TickFor(flow, game::kDyingSeconds);
	CHECK(flow.stage == Stage::Ready);
	CHECK(flow.lives == game::kStartingLives - 1);
	CHECK(flow.level == 1);
}

TEST_CASE("Being caught on the last life ends the game", "[game][gameflow]")
{
	GameFlow flow = game::CatchPlayer(In(Stage::Playing, 1));
	CHECK(flow.stage == Stage::Dying);
	CHECK(flow.lives == 0);

	flow = TickFor(flow, game::kDyingSeconds);
	CHECK(flow.stage == Stage::GameOver);
}

TEST_CASE("The player can only be caught while playing", "[game][gameflow]")
{
	for (const Stage stage : {Stage::Ready, Stage::Dying, Stage::LevelComplete, Stage::GameOver})
	{
		const GameFlow flow = In(stage, 2);
		const GameFlow after = game::CatchPlayer(flow);
		CHECK(after.stage == stage);
		CHECK(after.lives == 2);
	}
}

TEST_CASE("Clearing the level while playing completes it, then the next level gets ready", "[game][gameflow]")
{
	GameFlow flow = game::ClearLevel(In(Stage::Playing, 2, 1));
	CHECK(flow.stage == Stage::LevelComplete);
	CHECK(flow.level == 1);

	flow = game::Tick(flow, game::kLevelCompleteSeconds / 2.0f);
	CHECK(flow.stage == Stage::LevelComplete);

	flow = TickFor(flow, game::kLevelCompleteSeconds);
	CHECK(flow.stage == Stage::Ready);
	CHECK(flow.level == 2);
	CHECK(flow.lives == 2); // lives carry over

	CHECK(TickFor(flow, game::kReadySeconds).stage == Stage::Playing);
}

TEST_CASE("A level can only be cleared while playing", "[game][gameflow]")
{
	CHECK(game::ClearLevel(In(Stage::Ready)).stage == Stage::Ready);
	CHECK(game::ClearLevel(In(Stage::Dying)).stage == Stage::Dying);
	CHECK(game::ClearLevel(In(Stage::GameOver, 0)).stage == Stage::GameOver);

	const GameFlow complete = game::Tick(In(Stage::LevelComplete), 1.0f);
	CHECK(game::ClearLevel(complete).stageSeconds == complete.stageSeconds); // the timer isn't restarted
}

TEST_CASE("Confirm after GameOver starts a new game", "[game][gameflow]")
{
	const GameFlow flow = game::Confirm(In(Stage::GameOver, 0, 4));
	CHECK(flow.stage == Stage::Ready);
	CHECK(flow.lives == game::kStartingLives);
	CHECK(flow.level == 1);

	CHECK(game::Confirm(In(Stage::Playing, 2, 3)).stage == Stage::Playing);
	CHECK(game::Confirm(In(Stage::Ready, 2, 3)).level == 3);
}

TEST_CASE("Entering Ready resets what the previous stage calls for", "[game][gameflow]")
{
	using game::ResetFor;

	const game::RoundReset afterDying = ResetFor(Stage::Dying, Stage::Ready);
	CHECK(afterDying.actors);
	CHECK_FALSE(afterDying.pellets); // eaten pellets stay eaten
	CHECK_FALSE(afterDying.score);

	const game::RoundReset afterLevel = ResetFor(Stage::LevelComplete, Stage::Ready);
	CHECK(afterLevel.actors);
	CHECK(afterLevel.pellets);
	CHECK_FALSE(afterLevel.score);

	const game::RoundReset newGame = ResetFor(Stage::GameOver, Stage::Ready);
	CHECK(newGame.actors);
	CHECK(newGame.pellets);
	CHECK(newGame.score);

	for (const Stage to : {Stage::Playing, Stage::Dying, Stage::LevelComplete, Stage::GameOver})
	{
		const game::RoundReset none = ResetFor(Stage::Playing, to);
		CHECK_FALSE(none.actors);
		CHECK_FALSE(none.pellets);
		CHECK_FALSE(none.score);
	}
}
