#include "gameflow.hpp"

namespace game
{
	bool RoundTimeUp(Round round, float seconds) noexcept
	{
		switch (round)
		{
		case Round::Ready:
			return seconds >= kReadySeconds;
		case Round::Dying:
			return seconds >= kDyingSeconds;
		case Round::Active:
			break;
		}
		return false;
	}

	Screen ScreenAfterGame(int score, int record) noexcept
	{
		return score > record ? Screen::NameEntry : Screen::GameOver;
	}
} // namespace game
