#include "application/application.hpp"

#include <SDL3/SDL.h>
#include <optional>

#include "core/fixedtimestep.hpp"
#include "core/sdllog.hpp"
#include "input/input.hpp"
#include "input/mouse.hpp"

namespace engine::application
{
	namespace
	{
		struct KeyMapping
		{
			SDL_Scancode scancode;
			input::Key key;
		};

		// Built from the one key table (input/keys.hpp), so a new key needs no change here. A table
		// rather than a switch: SDL_Scancode has hundreds of enumerators and /Wall warns about every
		// one a switch leaves unhandled.
#define ENGINE_KEY_MAPPING(name, sdl) {SDL_SCANCODE_##sdl, input::Key::name},
		constexpr KeyMapping kKeyMap[] = {ENGINE_KEY_LIST(ENGINE_KEY_MAPPING)
		                                      ENGINE_KEY_ALIAS_LIST(ENGINE_KEY_MAPPING)};
#undef ENGINE_KEY_MAPPING

		// True if another physical key that counts as key (the other Shift, say) is still held, so
		// letting go of one of a pair doesn't release the key while the other is down.
		bool OtherPhysicalKeyHeld(input::Key key, SDL_Scancode released)
		{
			const bool* state = SDL_GetKeyboardState(nullptr);
			if (state == nullptr) return false;
			for (const KeyMapping& mapping : kKeyMap)
			{
				if (mapping.key == key && mapping.scancode != released && state[mapping.scancode]) return true;
			}
			return false;
		}

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

		std::optional<input::MouseButton> ToMouseButton(Uint8 button)
		{
			switch (button)
			{
			case SDL_BUTTON_LEFT:
				return input::MouseButton::Left;
			case SDL_BUTTON_RIGHT:
				return input::MouseButton::Right;
			case SDL_BUTTON_MIDDLE:
				return input::MouseButton::Middle;
			default:
				return std::nullopt;
			}
		}
	} // namespace

	Application::SdlScope::SdlScope()
	{
		// Subsystems (video, audio) are started by the classes that use them;
		// this only initialises SDL's core so SDL_Quit below has something to pair with.
		core::RouteSdlLog(); // once the platform code starts, SDL's log goes through the engine's
		if (!SDL_Init(0))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
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

		// SDL3 sends no SDL_EVENT_TEXT_INPUT until text input is started on the window.
		if (!SDL_StartTextInput(window.GetWindowHandle()))
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_StartTextInput failed, no typed text: %s", SDL_GetError());
		}

		core::FixedTimestep timestep(updateStep);
		double previous = clock.Now();

		while (PumpEvents() && !game.WantsToQuit())
		{
			const double now = clock.Now();
			const int updates = timestep.Advance(now - previous);
			previous = now;

			for (int i = 0; i < updates; ++i)
			{
				game.Update(timestep.Step(), input, &audio);
				input.EndUpdate();
			}

			game.Render(renderer, timestep.Alpha());
			renderer.Present();
		}

		return 0;
	}

	void Application::SetMousePosition(float x, float y)
	{
		int width = 0;
		int height = 0;
		SDL_GetRenderLogicalPresentation(renderer.GetRendererHandle(), &width, &height, nullptr);
		const bool inside = x >= 0 && y >= 0 && x < static_cast<float>(width) && y < static_cast<float>(height);
		input.mouse.OnMove({x, y}, inside);
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
					input.keyboard.OnKeyDown(*key);
				}
				break;
			case SDL_EVENT_KEY_UP:
				if (const std::optional<input::Key> key = ToKey(event.key.scancode))
				{
					if (!OtherPhysicalKeyHeld(*key, event.key.scancode)) input.keyboard.OnKeyUp(*key);
				}
				break;
			case SDL_EVENT_TEXT_INPUT:
				if (event.text.text != nullptr) input.OnText(event.text.text);
				break;
			case SDL_EVENT_MOUSE_MOTION:
				// Window pixels to logical pixels, so letterboxing and scaling are handled.
				SDL_ConvertEventToRenderCoordinates(renderer.GetRendererHandle(), &event);
				SetMousePosition(event.motion.x, event.motion.y);
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				SDL_ConvertEventToRenderCoordinates(renderer.GetRendererHandle(), &event);
				SetMousePosition(event.button.x, event.button.y);
				if (const std::optional<input::MouseButton> button = ToMouseButton(event.button.button))
				{
					input.mouse.OnButton(*button, event.button.down);
				}
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				// Notches, positive away from the user, whatever the OS's "natural scrolling" says.
				input.mouse.OnWheel(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y);
				break;
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				input.mouse.inside = false;
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				// Key-ups for keys released while unfocused never arrive; don't leave them stuck.
				input.ReleaseAll();
				break;
			default:
				break;
			}
		}
		return open;
	}
} // namespace engine::application
