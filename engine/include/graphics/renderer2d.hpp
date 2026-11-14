#pragma once
#include <cstddef>
#include <vector>

#include "graphics/camera.hpp"
#include "graphics/color.hpp"
#include "graphics/shapebatch.hpp"

// Only forward-declared: see renderer.hpp. The vector below is defined in renderer2d.cpp,
// where SDL_Vertex is complete, which is why the constructor and destructor live there too.
struct SDL_Vertex;

namespace engine::ecs
{
	class Registry;
}

namespace engine::graphics
{
	class Renderer;

	// Draws every Transform + Shape entity in one batched call. The game owns one and calls
	// Draw from its Render, after Clear; Application presents the frame afterwards.
	class Renderer2D
	{
		ShapeBatch batch;
		std::vector<SDL_Vertex> sdlVertices; // reused each frame
		Color tint{255, 255, 255};

	public:
		Camera camera;

		explicit Renderer2D(const Camera& initialCamera);
		~Renderer2D();

		Renderer2D(const Renderer2D&) = delete;
		Renderer2D& operator=(const Renderer2D&) = delete;
		Renderer2D(Renderer2D&&) = delete;
		Renderer2D& operator=(Renderer2D&&) = delete;

		// Multiplies every shape's colour, e.g. {128, 128, 128} to dim the scene by half.
		// White (the default) leaves colours unchanged.
		void SetTint(Color color) noexcept { tint = color; }
		[[nodiscard]] Color Tint() const noexcept { return tint; }

		// Only reads the registry; see ShapeBatch::Build.
		void Draw(Renderer& renderer, ecs::Registry& registry);

		// Shapes drawn by the last Draw, after culling.
		[[nodiscard]] std::size_t ShapeCount() const noexcept { return batch.ShapeCount(); }
	};
} // namespace engine::graphics
