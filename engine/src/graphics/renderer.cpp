#include "graphics/renderer.hpp"

#include <SDL3/SDL.h>

namespace engine::graphics
{
	Renderer::Renderer(const char* title, int width, int height)
	{
		if (!SDL_Init(SDL_INIT_VIDEO))
		{
			SDL_Log("SDL_Init failed: %s", SDL_GetError());
			return;
		}

		if (!SDL_CreateWindowAndRenderer(title, width, height, SDL_WINDOW_RESIZABLE, &window, &renderer))
		{
			SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
			window = nullptr;
			renderer = nullptr;
		}
	}

	Renderer::~Renderer()
	{
		if (renderer != nullptr)
		{
			SDL_DestroyRenderer(renderer);
		}
		if (window != nullptr)
		{
			SDL_DestroyWindow(window);
		}
		SDL_Quit();
	}

	bool Renderer::PollEvents()
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

	void Renderer::Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b)
	{
		SDL_SetRenderDrawColor(renderer, r, g, b, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);
	}

	void Renderer::Present()
	{
		SDL_RenderPresent(renderer);
	}
} // namespace engine::graphics
