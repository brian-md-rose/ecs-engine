#include "graphics/renderer2d.hpp"

#include <SDL3/SDL.h>

#include "graphics/renderer.hpp"

namespace engine::graphics
{
	namespace
	{
		// One channel of colour * tint, in SDL's 0-1 range.
		float Channel(std::uint8_t colour, std::uint8_t tint)
		{
			return static_cast<float>(colour) * static_cast<float>(tint) / (255.0f * 255.0f);
		}
	} // namespace

	Renderer2D::Renderer2D(const Camera& camera) : camera(camera)
	{
	}

	Renderer2D::~Renderer2D() = default;

	void Renderer2D::Draw(Renderer& renderer, ecs::Registry& registry)
	{
		batch.Build(registry, camera);
		if (batch.Indices().empty())
		{
			return;
		}

		sdlVertices.clear();
		for (const Vertex& vertex : batch.Vertices())
		{
			const SDL_FColor colour{Channel(vertex.color.r, tint.r), Channel(vertex.color.g, tint.g),
			                        Channel(vertex.color.b, tint.b), 1.0f};
			sdlVertices.push_back({{vertex.position.x, vertex.position.y}, colour, {0.0f, 0.0f}});
		}

		if (!SDL_RenderGeometry(renderer.GetRendererHandle(), nullptr, sdlVertices.data(),
		                        static_cast<int>(sdlVertices.size()), batch.Indices().data(),
		                        static_cast<int>(batch.Indices().size())))
		{
			SDL_Log("SDL_RenderGeometry failed: %s", SDL_GetError());
		}
	}
} // namespace engine::graphics
