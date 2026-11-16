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
	// Each fixed update runs Input, Update and LateUpdate in that order; each frame runs Render.
	class ScheduledGame : public Game
	{
	public:
		ecs::World world;
		ecs::Scheduler scheduler;

		void Update(double dt, const input::Keyboard& keyboard, audio::AudioDevice* audio) override;

		// An update with no sound, for tests.
		void Update(double dt, const input::Keyboard& keyboard) { Update(dt, keyboard, nullptr); }
		void Render(graphics::Renderer& renderer, double alpha) override;
	};
} // namespace engine::application
