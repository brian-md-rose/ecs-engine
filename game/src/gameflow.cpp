#include "gameflow.hpp"

namespace game
{
	namespace
	{
		[[nodiscard]] GameFlow Enter(GameFlow flow, Stage stage) noexcept
		{
			flow.stage = stage;
			flow.stageSeconds = 0.0f;
			return flow;
		}
	} // namespace

	GameFlow Tick(GameFlow flow, float seconds) noexcept
	{
		flow.stageSeconds += seconds;
		switch (flow.stage)
		{
		case Stage::Ready:
			if (flow.stageSeconds >= kReadySeconds) return Enter(flow, Stage::Playing);
			break;
		case Stage::Dying:
			if (flow.stageSeconds >= kDyingSeconds) return Enter(flow, flow.lives > 0 ? Stage::Ready : Stage::GameOver);
			break;
		case Stage::LevelComplete:
			if (flow.stageSeconds >= kLevelCompleteSeconds)
			{
				++flow.level;
				return Enter(flow, Stage::Ready);
			}
			break;
		case Stage::Title:
		case Stage::Playing:
		case Stage::GameOver:
			break;
		}
		return flow;
	}

	GameFlow CatchPlayer(GameFlow flow) noexcept
	{
		if (flow.stage != Stage::Playing) return flow;
		--flow.lives;
		return Enter(flow, Stage::Dying);
	}

	GameFlow ClearLevel(GameFlow flow) noexcept
	{
		if (flow.stage != Stage::Playing) return flow;
		return Enter(flow, Stage::LevelComplete);
	}

	GameFlow Confirm(GameFlow flow) noexcept
	{
		if (flow.stage != Stage::GameOver) return flow;
		return GameFlow{.stage = Stage::Title};
	}

	GameFlow StartGame(GameFlow flow) noexcept
	{
		if (flow.stage != Stage::Title) return flow;
		return GameFlow{};
	}

	GameFlow ReturnToTitle(GameFlow) noexcept
	{
		return GameFlow{.stage = Stage::Title};
	}

	RoundReset ResetFor(Stage from, Stage to) noexcept
	{
		if (from == to) return {};
		if (to == Stage::Title) return {.actors = true};
		if (to != Stage::Ready) return {};
		const bool newGame = from == Stage::Title || from == Stage::GameOver;
		return {.pellets = newGame || from == Stage::LevelComplete, .actors = true, .score = newGame};
	}
} // namespace game
