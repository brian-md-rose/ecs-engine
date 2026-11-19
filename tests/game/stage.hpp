#pragma once

#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "mazegame.hpp"

namespace gametest
{
	// The game's flow as one value, the way the old state machine had it: the screen, and while
	// Playing the round. Tests read it to wait for "Ready" or "Dying" without caring that they
	// are two states in the engine.
	enum class Stage
	{
		Title,
		Options,
		Help,
		UiDemo,
		NameEntry,
		Ready,
		Playing,
		Dying,
		LevelTransition,
		GameOver
	};

	[[nodiscard]] inline Stage StageOf(const game::MazeGame& maze)
	{
		using engine::ecs::State;
		switch (maze.world.resources.Get<State<game::Screen>>().value)
		{
		case game::Screen::Title:
			return Stage::Title;
		case game::Screen::Options:
			return Stage::Options;
		case game::Screen::Help:
			return Stage::Help;
		case game::Screen::UiDemo:
			return Stage::UiDemo;
		case game::Screen::NameEntry:
			return Stage::NameEntry;
		case game::Screen::LevelTransition:
			return Stage::LevelTransition;
		case game::Screen::GameOver:
			return Stage::GameOver;
		case game::Screen::Playing:
			break;
		}
		switch (maze.world.resources.Get<State<game::Round>>().value)
		{
		case game::Round::Ready:
			return Stage::Ready;
		case game::Round::Dying:
			return Stage::Dying;
		case game::Round::Active:
			break;
		}
		return Stage::Playing;
	}

	[[nodiscard]] inline bool IsPaused(const game::MazeGame& maze)
	{
		return maze.world.resources.Get<engine::ecs::State<game::Pause>>().Is(game::Pause::Paused);
	}
	// Starts a game the way the title screen would, without the keys: the change takes effect on
	// the next update.
	inline void RequestPlaying(game::MazeGame& maze)
	{
		maze.world.resources.Get<engine::ecs::NextState<game::Screen>>().Set(game::Screen::Playing);
	}
} // namespace gametest
