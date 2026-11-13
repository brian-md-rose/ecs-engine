#pragma once

#include "core/clock.hpp"
#include "graphics/renderer.hpp"
#include "graphics/window.hpp"
#include "input/keyboard.hpp"

namespace engine::application
{
	struct ApplicationConfig
	{
		const char* title = "ecs-engine";
		int windowWidth = 1280;
		int windowHeight = 720;
		int logicalWidth = 320; // the game's fixed resolution, integer-scaled to the window
		int logicalHeight = 180;
		double updateRate = 60.0; // fixed game-logic updates per second
	};

	// What the game provides. Application calls these from its main loop.
	class Game
	{
	public:
		virtual ~Game() = default;

		// Called at a fixed rate (1 / ApplicationConfig::updateRate seconds, passed as dt),
		// zero or more times per frame. Run ECS systems here. The keyboard's pressed/released
		// edges are cleared after each call, so each press is seen by exactly one update.
		// Mapping keys to game actions is the game's job.
		virtual void Update(double dt, const input::Keyboard& keyboard) = 0;

		// Called once per frame: clear and draw. Application presents the frame afterwards.
		// alpha in [0, 1) is how far the game is into the next update, for optional
		// interpolation between the last two states.
		virtual void Render(graphics::Renderer& renderer, double alpha) = 0;
	};

	// Owns the platform (SDL, window, renderer) and runs the main loop.
	class Application
	{
		// Starts SDL before the window exists and shuts it down after the window is gone.
		class SdlScope
		{
		public:
			SdlScope();
			~SdlScope();
			SdlScope(const SdlScope&) = delete;
			SdlScope& operator=(const SdlScope&) = delete;
			SdlScope(SdlScope&&) = delete;
			SdlScope& operator=(SdlScope&&) = delete;
		};

		// Declaration order is construction order; destruction runs in reverse, so the
		// renderer goes first, then the window, and SDL shuts down last.
		SdlScope sdl;
		graphics::Window window;
		graphics::Renderer renderer;
		input::Keyboard keyboard;
		double updateStep;

		// Drains pending SDL events, feeding key events to keyboard. Returns false once the
		// window has been asked to close.
		bool PumpEvents();

	public:
		explicit Application(const ApplicationConfig& config);

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;
		Application(Application&&) = delete;
		Application& operator=(Application&&) = delete;

		// Runs until the window is closed. Returns 0 on a normal exit, 1 if the window or
		// renderer couldn't be created.
		int Run(Game& game);

		// As above, with a caller-supplied clock (for tests or replays).
		int Run(Game& game, const core::Clock& clock);
	};
} // namespace engine::application
