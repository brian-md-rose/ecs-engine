#include "application/application.hpp"

#include <SDL3/SDL.h>

#include "core/fixedtimestep.hpp"

namespace engine::application
{
	Application::SdlScope::SdlScope()
	{
		// Subsystems (video, later input/audio) are started by the classes that use them;
		// this only initialises SDL's core so SDL_Quit below has something to pair with.
		if (!SDL_Init(0))
		{
			SDL_Log("SDL_Init failed: %s", SDL_GetError());
		}
	}

	Application::SdlScope::~SdlScope()
	{
		SDL_Quit();
	}

	Application::Application(const ApplicationConfig& config)
	    : window(config.title, config.windowWidth, config.windowHeight),
	      renderer(window, config.logicalWidth, config.logicalHeight), updateStep(1.0 / config.updateRate)
	{
	}

	int Application::Run(Game& game)
	{
		const core::SystemClock clock;
		return Run(game, clock);
	}

	int Application::Run(Game& game, const core::Clock& clock)
	{
		if (!window.IsValid() || !renderer.IsValid())
		{
			return 1;
		}

		core::FixedTimestep timestep(updateStep);
		double previous = clock.Now();

		while (window.PollEvents())
		{
			const double now = clock.Now();
			const int updates = timestep.Advance(now - previous);
			previous = now;

			for (int i = 0; i < updates; ++i)
			{
				game.Update(timestep.Step());
			}

			game.Render(renderer, timestep.Alpha());
			renderer.Present();
		}

		return 0;
	}
} // namespace engine::application
