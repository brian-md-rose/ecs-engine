#pragma once

#include "application/application.hpp"
#include "ecs/world.hpp"
#include "graphics/renderer.hpp"
#include "input/keyboard.hpp"

namespace game
{
	// Portrait, the shape of a classic maze screen, scaled kWindowScale times into the window.
	inline constexpr int kLogicalWidth = 224;
	inline constexpr int kLogicalHeight = 288;
	inline constexpr int kWindowScale = 3;

	// Test bed for rendering and input: a square steered with the arrow keys or WASD.
	// P or Escape pauses; F1 toggles the rendering stress test. Systems are still called by
	// hand here; the scheduler (#14) takes that over.
	class MazeGame final : public engine::application::Game
	{
		engine::ecs::World world;

	public:
		MazeGame();

		void Update(double dt, const engine::input::Keyboard& keyboard) override;
		void Render(engine::graphics::Renderer& renderer, double alpha) override;
	};
} // namespace game
