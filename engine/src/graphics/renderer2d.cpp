#include "graphics/renderer2d.hpp"

#include <SDL3/SDL.h>
#include <chrono>
#include <utility>

#include "graphics/renderer.hpp"

namespace engine::graphics
{
	namespace
	{
		// One channel in SDL's 0-1 range.
		float Channel(std::uint8_t colour)
		{
			return static_cast<float>(colour) / 255.0f;
		}
	} // namespace

	Renderer2D::Renderer2D(const Camera& initialCamera) : camera(initialCamera)
	{
	}

	Renderer2D::~Renderer2D() = default;

	void Renderer2D::SetAtlas(Atlas newAtlas)
	{
		atlas = std::move(newAtlas);
		uploadPending = true;
	}

	void Renderer2D::SetFont(const BitmapFont& newFont)
	{
		font = newFont;
		hasFont = true;
	}

	void Renderer2D::Draw(Renderer& renderer, ecs::Registry& registry, float alpha)
	{
		const auto start = std::chrono::steady_clock::now();
		Submit(renderer, registry, alpha);
		lastDrawTime = std::chrono::steady_clock::now() - start;
	}

	void Renderer2D::Submit(Renderer& renderer, ecs::Registry& registry, float alpha)
	{
		if (uploadPending)
		{
			renderer.DestroyTexture(texture);
			texture = renderer.CreateTexture(atlas.image);
			uploadPending = false;
		}
		SDL_Texture* handle = renderer.GetTextureHandle(texture);

		// Without a texture, sprites and text can't be drawn: build the batch with no atlas.
		BatchSettings settings{.camera = camera, .tint = tint, .untintedFromLayer = untintedFromLayer, .alpha = alpha};
		if (handle != nullptr)
		{
			settings.atlasWidth = atlas.image.Width();
			settings.atlasHeight = atlas.image.Height();
			settings.solid = atlas.solid;
			settings.font = hasFont ? &font : nullptr;
		}
		batch.Build(registry, settings);
		if (batch.Indices().empty())
		{
			return;
		}

		sdlVertices.clear();
		for (const Vertex& vertex : batch.Vertices())
		{
			const SDL_FColor colour{Channel(vertex.color.r), Channel(vertex.color.g), Channel(vertex.color.b), 1.0f};
			sdlVertices.push_back({{vertex.position.x, vertex.position.y}, colour, {vertex.uv.x, vertex.uv.y}});
		}

		// One draw call per stretch of the same clip rectangle; usually just one. The clip is in logical
		// pixels, like everything else drawn here, and is switched off again afterwards.
		SDL_Renderer* sdlRenderer = renderer.GetRendererHandle();
		const int indexCount = static_cast<int>(batch.Indices().size());
		for (const DrawBatch::Run& run : batch.Runs())
		{
			const SDL_Rect rect{run.clip.x, run.clip.y, run.clip.width, run.clip.height};
			if (!SDL_SetRenderClipRect(sdlRenderer, run.clipped ? &rect : nullptr))
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_SetRenderClipRect failed: %s", SDL_GetError());
			}
			if (run.firstIndex + run.indexCount > indexCount) break;
			if (!SDL_RenderGeometry(sdlRenderer, handle, sdlVertices.data(), static_cast<int>(sdlVertices.size()),
			                        batch.Indices().data() + run.firstIndex, run.indexCount))
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RenderGeometry failed: %s", SDL_GetError());
			}
		}
		SDL_SetRenderClipRect(sdlRenderer, nullptr);
	}
} // namespace engine::graphics
