#include "application/application.hpp"

#include <SDL3/SDL.h>

#include <optional>

#include "core/fixedtimestep.hpp"

namespace engine::application
{
	namespace
	{
		struct KeyMapping
		{
			SDL_Scancode scancode;
			input::Key key;
		};

		// A table rather than a switch: SDL_Scancode has hundreds of enumerators and /Wall
		// warns about every one a switch leaves unhandled.
		constexpr KeyMapping kKeyMap[] =
		{
		    {SDL_SCANCODE_UP, input::Key::Up},        {SDL_SCANCODE_DOWN, input::Key::Down},
		    {SDL_SCANCODE_LEFT, input::Key::Left},    {SDL_SCANCODE_RIGHT, input::Key::Right},
		    {SDL_SCANCODE_W, input::Key::W},          {SDL_SCANCODE_A, input::Key::A},
		    {SDL_SCANCODE_S, input::Key::S},          {SDL_SCANCODE_D, input::Key::D},
		    {SDL_SCANCODE_P, input::Key::P},          {SDL_SCANCODE_SPACE, input::Key::Space},
		    {SDL_SCANCODE_RETURN, input::Key::Enter}, {SDL_SCANCODE_ESCAPE, input::Key::Escape},
		};

		std::optional<input::Key> ToKey(SDL_Scancode scancode)
		{
			for (const KeyMapping& mapping : kKeyMap)
			{
				if (mapping.scancode == scancode)
				{
					return mapping.key;
				}
			}
			return std::nullopt;
		}
	} // namespace

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

		while (PumpEvents())
		{
			const double now = clock.Now();
			const int updates = timestep.Advance(now - previous);
			previous = now;

			for (int i = 0; i < updates; ++i)
			{
				game.Update(timestep.Step(), keyboard);
				keyboard.EndUpdate();
			}

			game.Render(renderer, timestep.Alpha());
			renderer.Present();
		}

		return 0;
	}

	bool Application::PumpEvents()
	{
		bool open = true;
		SDL_Event event{};
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				open = false;
				break;
			case SDL_EVENT_KEY_DOWN:
				if (const std::optional<input::Key> key = ToKey(event.key.scancode))
				{
					keyboard.OnKeyDown(*key);
				}
				break;
			case SDL_EVENT_KEY_UP:
				if (const std::optional<input::Key> key = ToKey(event.key.scancode))
				{
					keyboard.OnKeyUp(*key);
				}
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				// Key-ups for keys released while unfocused never arrive; don't leave them stuck.
				keyboard.ReleaseAll();
				break;
			default:
				break;
			}
		}
		return open;
	}
} // namespace engine::application
