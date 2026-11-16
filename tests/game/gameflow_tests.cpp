#include <catch2/catch_test_macros.hpp>

#include "gameflow.hpp"

TEST_CASE("A game starts on level 1 with every life and the clock at 0", "[game][gameflow]")
{
	const game::GameFlow flow;
	CHECK(flow.lives == game::kStartingLives);
	CHECK(flow.level == 1);
	CHECK(flow.seconds == 0.0f);
}

TEST_CASE("Ready and Dying end after their time, and Active has no timer", "[game][gameflow]")
{
	using game::Round;
	CHECK_FALSE(game::RoundTimeUp(Round::Ready, game::kReadySeconds - 0.01f));
	CHECK(game::RoundTimeUp(Round::Ready, game::kReadySeconds));
	CHECK_FALSE(game::RoundTimeUp(Round::Dying, game::kDyingSeconds - 0.01f));
	CHECK(game::RoundTimeUp(Round::Dying, game::kDyingSeconds));
	CHECK_FALSE(game::RoundTimeUp(Round::Active, 3600.0f));
}

TEST_CASE("A game goes to the name entry only if its score beats the record", "[game][gameflow]")
{
	CHECK(game::ScreenAfterGame(101, 100) == game::Screen::NameEntry);
	CHECK(game::ScreenAfterGame(100, 100) == game::Screen::GameOver);
	CHECK(game::ScreenAfterGame(50, 100) == game::Screen::GameOver);
	CHECK(game::ScreenAfterGame(5, 0) == game::Screen::NameEntry);
	CHECK(game::ScreenAfterGame(0, 0) == game::Screen::GameOver);
}
