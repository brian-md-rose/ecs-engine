#pragma once
#include <chrono>
#include <cstddef>
#include <vector>

#include "graphics/atlas.hpp"
#include "graphics/camera.hpp"
#include "graphics/color.hpp"
#include "graphics/drawbatch.hpp"
#include "graphics/font.hpp"
#include "graphics/renderer.hpp"

// Only forward-declared: see renderer.hpp. The vector below is defined in renderer2d.cpp,
// where SDL_Vertex is complete, which is why the constructor and destructor live there too.
struct SDL_Vertex;

namespace engine::ecs
{
	class Registry;
}

namespace engine::graphics
{
	// Draws every shape, sprite and text entity in one batched call, from one atlas texture.
	// The game owns one, gives it an atlas and a font at setup, and calls Draw from its Render,
	// after Clear; Application presents the frame afterwards.
	//
	// The atlas is kept as an Image and uploaded to the Renderer on the first Draw after
	// SetAtlas, so a game can be set up (and tested) before any renderer exists. The texture
	// belongs to the Renderer, so it is never used after the Renderer is gone.
	class Renderer2D
	{
		DrawBatch batch;
		std::vector<SDL_Vertex> sdlVertices; // reused each frame
		Atlas atlas;
		BitmapFont font;
		bool hasFont = false;
		TextureId texture = kNoTexture;
		bool uploadPending = false;
		Color tint{255, 255, 255};
		int untintedFromLayer = 256;
		std::chrono::steady_clock::duration lastDrawTime{};

		// Draw without the timing.
		void Submit(Renderer& renderer, ecs::Registry& registry, float alpha);

	public:
		Camera camera;

		explicit Renderer2D(const Camera& initialCamera);
		~Renderer2D();

		Renderer2D(const Renderer2D&) = delete;
		Renderer2D& operator=(const Renderer2D&) = delete;
		Renderer2D(Renderer2D&&) = delete;
		Renderer2D& operator=(Renderer2D&&) = delete;

		// The atlas sprites and text are cut from (see PackAtlas). Until one is set, only shapes
		// are drawn. Always draw with the same Renderer: the texture is made on it.
		void SetAtlas(Atlas newAtlas);
		[[nodiscard]] const Atlas& GetAtlas() const noexcept { return atlas; }

		// The font Text components are drawn with; its glyphs are in the atlas.
		void SetFont(const BitmapFont& newFont);
		[[nodiscard]] const BitmapFont* GetFont() const noexcept { return hasFont ? &font : nullptr; }

		// Multiplies the colour of everything below untintedFrom, e.g. {128, 128, 128} to dim
		// the scene by half while a menu on a higher layer stays bright. White (the default)
		// leaves colours unchanged.
		void SetTint(Color color, int untintedFrom = 256) noexcept
		{
			tint = color;
			untintedFromLayer = untintedFrom;
		}
		[[nodiscard]] Color Tint() const noexcept { return tint; }

		// Only reads the registry; see DrawBatch::Build. alpha is the Render phase's
		// FrameContext::alpha, for entities with an Interpolation (see BatchSettings::alpha).
		void Draw(Renderer& renderer, ecs::Registry& registry, float alpha = 1.0f);

		// Shapes, sprites and characters drawn by the last Draw, after culling.
		[[nodiscard]] std::size_t ItemCount() const noexcept { return batch.ItemCount(); }

		// How long the last Draw took, building the batch and handing it to the GPU.
		[[nodiscard]] std::chrono::steady_clock::duration LastDrawTime() const noexcept { return lastDrawTime; }
	};
} // namespace engine::graphics
