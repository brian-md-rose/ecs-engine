#include "gamestates.hpp"

#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "save.hpp"
#include "screens.hpp"
#include "systems.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::FrameContext;
		using engine::ecs::World;

		void EnterTitle(World& world, const FrameContext&)
		{
			ResetActors(world); // the title shows the maze at rest
		}

		void BeginLevelTransition(World& world, const FrameContext&)
		{
			GameFlow& flow = world.resources.Get<GameFlow>();
			++flow.level;
			flow.seconds = 0.0f;
		}

		void ExitLevelTransition(World& world, const FrameContext&)
		{
			ResetPellets(world); // the cleared maze stays in view until the next level begins
		}

		void ExitOptions(World& world, const FrameContext&)
		{
			SaveSettings(world);
		}

		void EnterReady(World& world, const FrameContext&)
		{
			world.resources.Get<GameFlow>().seconds = 0.0f;
			ResetActors(world);
		}

		void EnterDying(World& world, const FrameContext&)
		{
			world.resources.Get<GameFlow>().seconds = 0.0f;
		}
	} // namespace

	void AddGameStates(World& world, engine::ecs::Scheduler& scheduler)
	{
		scheduler.AddState(world, Screen::Title);
		scheduler.AddSubState(world, Screen::Playing, Round::Ready);
		scheduler.AddSubState(world, Screen::Playing, Pause::Running);

		scheduler.AddOnEnter(Screen::Title, "EnterTitle", EnterTitle);
		scheduler.AddOnEnter(Screen::Title, "SpawnTitleScreen", SpawnTitleScreen);
		scheduler.AddOnEnter(Screen::Options, "SpawnOptionsScreen", SpawnOptionsScreen);
		scheduler.AddOnExit(Screen::Options, "SaveSettings", ExitOptions);
		scheduler.AddOnEnter(Screen::NameEntry, "SpawnNameEntryScreen", SpawnNameEntryScreen);
		scheduler.AddOnEnter(Screen::LevelTransition, "BeginLevelTransition", BeginLevelTransition);
		scheduler.AddOnEnter(Screen::LevelTransition, "SpawnLevelTransitionScreen", SpawnLevelTransitionScreen);
		scheduler.AddOnExit(Screen::LevelTransition, "ExitLevelTransition", ExitLevelTransition);

		scheduler.AddOnEnter(Round::Ready, "EnterReady", EnterReady);
		scheduler.AddOnEnter(Round::Dying, "EnterDying", EnterDying);
		scheduler.AddOnEnter(Pause::Paused, "SpawnPauseMenu", SpawnPauseMenu);
	}
} // namespace game
