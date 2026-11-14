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
		return GameFlow{};
	}

	RoundReset ResetFor(Stage from, Stage to) noexcept
	{
		if (to != Stage::Ready || from == Stage::Ready) return {};
		return {.pellets = from == Stage::LevelComplete || from == Stage::GameOver,
		        .actors = true,
		        .score = from == Stage::GameOver};
	}
} // namespace game
