#include "graphics/window.hpp"

#include <SDL3/SDL.h>
#include <utility>

namespace engine::graphics
{
	Window::Window(const char* title, int width, int height)
	{
		videoInitialized = SDL_InitSubSystem(SDL_INIT_VIDEO);
		if (!videoInitialized)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_InitSubSystem(VIDEO) failed: %s", SDL_GetError());
			return;
		}

		window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
		if (window == nullptr)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
		}
	}

	bool Window::SetMode(WindowMode mode, int width, int height)
	{
		if (window == nullptr) return false;

		bool ok = true;
		const auto check = [&ok](bool succeeded, const char* what)
		{
			if (!succeeded)
			{
				ok = false;
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s failed: %s", what, SDL_GetError());
			}
		};

		switch (mode)
		{
		case WindowMode::Windowed:
			check(SDL_SetWindowFullscreen(window, false), "SDL_SetWindowFullscreen(false)");
			check(SDL_SetWindowBordered(window, true), "SDL_SetWindowBordered");
			check(SDL_SetWindowSize(window, width, height), "SDL_SetWindowSize");
			break;
		case WindowMode::Borderless:
			// No mode asked for means the desktop's, which is borderless fullscreen.
			check(SDL_SetWindowFullscreenMode(window, nullptr), "SDL_SetWindowFullscreenMode(desktop)");
			check(SDL_SetWindowFullscreen(window, true), "SDL_SetWindowFullscreen(true)");
			break;
		case WindowMode::Fullscreen:
		{
			SDL_DisplayMode closest{};
			const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
			if (SDL_GetClosestFullscreenDisplayMode(display, width, height, 0.0f, true, &closest))
			{
				check(SDL_SetWindowFullscreenMode(window, &closest), "SDL_SetWindowFullscreenMode");
			}
			else
			{
				check(false, "SDL_GetClosestFullscreenDisplayMode");
			}
			check(SDL_SetWindowFullscreen(window, true), "SDL_SetWindowFullscreen(true)");
			break;
		}
		}
		return ok;
	}

	std::vector<Resolution> Window::Resolutions() const
	{
		if (window == nullptr) return {};

		int count = 0;
		SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetDisplayForWindow(window), &count);
		if (modes == nullptr) return {};

		std::vector<Resolution> sizes;
		for (int i = 0; i < count; ++i)
		{
			sizes.push_back({.width = modes[i]->w, .height = modes[i]->h});
		}
		SDL_free(static_cast<void*>(modes));
		return NormalizeResolutions(std::move(sizes));
	}

	Window::~Window()
	{
		if (window != nullptr)
		{
			SDL_DestroyWindow(window);
		}

		if (videoInitialized)
		{
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
		}
	}
} // namespace engine::graphics
