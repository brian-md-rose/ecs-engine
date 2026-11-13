#include "graphics/renderer.hpp"
#include "graphics/window.hpp"
#include <SDL3/SDL.h>

namespace engine::graphics
{
	Renderer::Renderer(Window& window, int renderWidth, int renderHeight)
	{
		if (!window.IsValid())
		{
			return;
		}

		renderer = SDL_CreateRenderer(window.GetWindowHandle(), nullptr);
		if (renderer == nullptr)
		{
			SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
			return;
		}

		if (!SDL_SetRenderLogicalPresentation(renderer, renderWidth, renderHeight,
		                                      SDL_LOGICAL_PRESENTATION_INTEGER_SCALE))
		{
			SDL_Log("SDL_SetRenderLogicalPresentation failed: %s", SDL_GetError());
		}

		if (!SDL_SetRenderVSync(renderer, 1))
		{
			SDL_Log("SDL_SetRenderVSync failed: %s", SDL_GetError());
		}
	}

	Renderer::~Renderer()
	{
		if (renderer != nullptr)
		{
			SDL_DestroyRenderer(renderer);
		}
	}

	void Renderer::Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b)
	{
		SDL_SetRenderDrawColor(renderer, r, g, b, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);
	}

	void Renderer::FillRect(float x, float y, float width, float height, std::uint8_t r, std::uint8_t g, std::uint8_t b)
	{
		const SDL_FRect rect{x, y, width, height};
		SDL_SetRenderDrawColor(renderer, r, g, b, SDL_ALPHA_OPAQUE);
		SDL_RenderFillRect(renderer, &rect);
	}

	void Renderer::Present()
	{
		SDL_RenderPresent(renderer);
	}
} // namespace engine::graphics
