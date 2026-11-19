#include "graphics/renderer.hpp"

#include <SDL3/SDL.h>
#include <cstddef>

#include "graphics/image.hpp"
#include "graphics/window.hpp"

namespace engine::graphics
{
	namespace
	{
		SDL_RendererLogicalPresentation PresentationOf(Scaling scaling) noexcept
		{
			return scaling == Scaling::Fit ? SDL_LOGICAL_PRESENTATION_LETTERBOX
			                               : SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
		}
	} // namespace

	Renderer::Renderer(Window& window, int renderWidth, int renderHeight, Scaling scalingMode)
	    : logicalWidth(renderWidth), logicalHeight(renderHeight), scaling(scalingMode)
	{
		if (!window.IsValid())
		{
			return;
		}

		renderer = SDL_CreateRenderer(window.GetWindowHandle(), nullptr);
		if (renderer == nullptr)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
			return;
		}

		SetScaling(scaling);
		SetVSync(true);
	}

	bool Renderer::SetScaling(Scaling mode)
	{
		if (renderer == nullptr) return false;
		if (!SDL_SetRenderLogicalPresentation(renderer, logicalWidth, logicalHeight, PresentationOf(mode)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_SetRenderLogicalPresentation failed: %s", SDL_GetError());
			return false;
		}
		scaling = mode;
		return true;
	}

	bool Renderer::SetVSync(bool enabled)
	{
		if (renderer == nullptr) return false;
		if (!SDL_SetRenderVSync(renderer, enabled ? 1 : 0))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_SetRenderVSync failed: %s", SDL_GetError());
			return false;
		}
		return true;
	}

	Renderer::~Renderer()
	{
		for (SDL_Texture* texture : textures)
		{
			if (texture != nullptr) SDL_DestroyTexture(texture);
		}
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

	TextureId Renderer::CreateTexture(const Image& image)
	{
		if (renderer == nullptr || image.IsEmpty()) return kNoTexture;

		// RGBA32 is R, G, B, A bytes in memory, whatever the CPU's byte order: the layout of Rgba.
		SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
		                                         image.Width(), image.Height());
		if (texture == nullptr)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateTexture failed: %s", SDL_GetError());
			return kNoTexture;
		}
		if (!SDL_UpdateTexture(texture, nullptr, image.Pixels().data(), image.Width() * 4))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_UpdateTexture failed: %s", SDL_GetError());
		}
		if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_SetTextureBlendMode failed: %s", SDL_GetError());
		}
		if (!SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_SetTextureScaleMode failed: %s", SDL_GetError());
		}

		textures.push_back(texture);
		return static_cast<TextureId>(textures.size());
	}

	void Renderer::DestroyTexture(TextureId texture)
	{
		if (texture == kNoTexture || texture > textures.size()) return;
		SDL_Texture*& handle = textures[texture - 1];
		if (handle != nullptr) SDL_DestroyTexture(handle);
		handle = nullptr;
	}

	SDL_Texture* Renderer::GetTextureHandle(TextureId texture) const noexcept
	{
		if (texture == kNoTexture || texture > textures.size()) return nullptr;
		return textures[texture - 1];
	}

	void Renderer::Present()
	{
		SDL_RenderPresent(renderer);
	}
} // namespace engine::graphics
