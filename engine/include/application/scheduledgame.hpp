#pragma once

#include "application/application.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"

namespace engine::application
{
	// A Game made of a World and the systems that run on it. A game derives from it and, in its
	// constructor, adds its resources and entities to world and registers its systems:
	//
	//     MyGame::MyGame()
	//     {
	//         world.resources.Emplace<Score>();
	//         scheduler.Add(ecs::Phase::Update, MovementSystem);
	//     }
	//
	// Each fixed update applies the state changes asked for (Scheduler::RunTransitions, see
	// ecs/states.hpp), records where Interpolation entities are (RecordPreviousPositions), then
	// runs Input, Update and LateUpdate in that order; each frame runs Render.
	class ScheduledGame : public Game
	{
	public:
		ecs::World world;
		ecs::Scheduler scheduler;

		void Update(double dt, const input::Input& input, audio::AudioDevice* audio) override;

		// An update with no sound, for tests.
		void Update(double dt, const input::Input& input) { Update(dt, input, nullptr); }
		void Render(graphics::Renderer& renderer, double alpha) override;

		// The world's DisplayRequest and DisplayInfo resources, if it has them (nullptr for each it lacks).
		[[nodiscard]] DisplayLink Display() override;
	};
} // namespace engine::application
