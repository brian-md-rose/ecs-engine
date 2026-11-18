#pragma once

#include "audio/audiodevice.hpp"
#include "core/clock.hpp"
#include "graphics/renderer.hpp"
#include "graphics/window.hpp"
#include "input/input.hpp"

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
		// zero or more times per frame. Run ECS systems here. The input's pressed/released
		// edges and typed text are cleared after each call, so each press is seen by exactly one update.
		// Mapping keys to game actions is the game's job. audio plays sound effects; it is null
		// when there is nothing to play them on (tests), and silent when there is no device.
		virtual void Update(double dt, const input::Input& input, audio::AudioDevice* audio) = 0;

		// Called once per frame: clear and draw. Application presents the frame afterwards.
		// alpha in [0, 1) is how far the game is into the next update, for optional
		// interpolation between the last two states.
		virtual void Render(graphics::Renderer& renderer, double alpha) = 0;

		// Asked after each update: true closes the application, as closing the window does (a
		// menu's Quit, for one). Games that never ask to quit needn't override it.
		[[nodiscard]] virtual bool WantsToQuit() const { return false; }
	};

	// Owns the platform (SDL, window, renderer, audio) and runs the main loop.
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

		// Declaration order is construction order; destruction runs in reverse, so the audio
		// device and renderer go first, then the window, and SDL shuts down last.
		SdlScope sdl;
		graphics::Window window;
		graphics::Renderer renderer;
		audio::AudioDevice audio;
		input::Input input;
		double updateStep;

		// Drains pending SDL events, feeding keyboard, text and mouse events to input. Returns false once the
		// window has been asked to close.
		bool PumpEvents();
		// Records the pointer at a logical-pixel position, and whether it is over the logical area.
		void SetMousePosition(float x, float y);

	public:
		explicit Application(const ApplicationConfig& config);

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;
		Application(Application&&) = delete;
		Application& operator=(Application&&) = delete;

		// Runs until the window is closed or the game asks to quit. Returns 0 on a normal exit, 1 if the window or
		// renderer couldn't be created.
		int Run(Game& game);

		// As above, with a caller-supplied clock (for tests or replays).
		int Run(Game& game, const core::Clock& clock);
	};
} // namespace engine::application
