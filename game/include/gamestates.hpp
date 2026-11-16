#pragma once

#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"

namespace game
{
	// Setup: adds the game's states (Screen, and Round and Pause inside Screen::Playing) and the
	// systems that run when each begins or ends (see gameflow.hpp for the states):
	//
	//   enter Title            actors at rest; the title menu
	//   enter Options          the options menu          exit Options    saves the settings
	//   enter NameEntry        the name-entry screen
	//   enter LevelTransition  the level counts up; the level screen
	//   exit LevelTransition   fresh pellets for the next level
	//   enter Round::Ready     actors back at their spawns, the Ready clock at 0
	//   enter Round::Dying     the Dying clock at 0
	//   enter Pause::Paused    the pause menu
	//
	// Every screen's entities carry DespawnOnExit, so leaving the screen removes them. The
	// game starts on the title.
	void AddGameStates(engine::ecs::World& world, engine::ecs::Scheduler& scheduler);
} // namespace game
