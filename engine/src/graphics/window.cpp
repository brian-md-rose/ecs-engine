#include "graphics/window.hpp"
#include <SDL3/SDL.h>

namespace engine::graphics
{
	Window::Window(const char* title, int width, int height)
	{
		videoInitialized = SDL_InitSubSystem(SDL_INIT_VIDEO);

		if (!videoInitialized)
		{
			SDL_Log("SDL_InitSubSystem(VIDEO) failed: %s", SDL_GetError());
			return;
		}

		window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
		if (window == nullptr)
		{
			SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		}
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

	bool Window::PollEvents()
	{
		bool open = true;
		SDL_Event event{};
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				open = false;
			}
		}
		return open;
	}
} // namespace engine::graphics
