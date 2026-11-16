#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "graphics/camera.hpp"
#include "graphics/color.hpp"
#include "graphics/font.hpp"
#include "graphics/image.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"

namespace engine::ecs
{
	class Registry;
}

namespace engine::graphics
{
	// A corner of a triangle, in screen (logical-pixel) coordinates, with the point of the atlas
	// it shows (0-1 across and down the atlas).
	struct Vertex
	{
		Position position;
		Color color;
		Position uv{};
	};

	// What a DrawBatch is built against.
	struct BatchSettings
	{
		Camera camera{};
		// The atlas sprites and text are cut from. Without one (0 x 0) only shapes are drawn.
		int atlasWidth = 0;
		int atlasHeight = 0;
		PixelRect solid{};                // an opaque white pixel of the atlas, which shapes sample
		const BitmapFont* font = nullptr; // for Text; without one, Text is skipped
		// Multiplies the colour of everything below untintedFromLayer, e.g. {128, 128, 128} to
		// dim the scene behind a pause screen. White (the default) leaves colours unchanged.
		Color tint{};
		int untintedFromLayer = 256;
		// How far this frame is between the last two updates, 0-1: entities with an Interpolation
		// are drawn that far from their previous position to their current one. 1 draws every
		// entity where it is now.
		float alpha = 1.0f;
	};

	// Turns every Transform + Shape, Transform + Sprite and Transform + Text entity into one
	// indexed triangle list over one atlas, so the whole scene can be drawn in a single call.
	// Pure: no SDL, so the tests can check it directly.
	//
	// Everything is drawn in ascending layer order; within a layer, shapes come first, then
	// sprites, then text, each in the registry's iteration order (kept stable by the sort).
	// Anything entirely outside the camera is skipped. Sprites and text are placed on whole
	// logical pixels, so pixel art stays sharp.
	class DrawBatch
	{
		struct Entry
		{
			Position screen{}; // top-left, camera applied
			std::uint8_t layer = 0;
			Color color{};
			bool textured = false; // a quad cut from the atlas; otherwise a shape
			Shape shape{};         // shapes
			PixelRect source{};    // textured quads
			int scale = 1;         // textured quads
			bool flipX = false;    // textured quads
		};

		std::vector<Entry> entries;
		std::vector<GlyphQuad> glyphs; // reused by text layout
		std::vector<Vertex> vertices;
		std::vector<int> indices; // int because SDL_RenderGeometry takes int indices

		void AddRectangle(const Entry& entry, Position uv);
		void AddCircle(const Entry& entry, Position uv);
		void AddQuad(const Entry& entry, const BatchSettings& settings);

	public:
		// Rim points per circle: enough to look round at 3-4x scale without many vertices.
		static constexpr int kCircleSegments = 24;

		// Rebuilds the batch from the registry. Only reads components; non-const because
		// Registry::Each is. The buffers are reused, so this doesn't allocate once warm.
		void Build(ecs::Registry& registry, const BatchSettings& settings);

		// Shapes only, with no atlas.
		void Build(ecs::Registry& registry, const Camera& camera);

		[[nodiscard]] std::span<const Vertex> Vertices() const noexcept { return vertices; }
		[[nodiscard]] std::span<const int> Indices() const noexcept { return indices; }

		// Shapes, sprites and characters that made it into the batch (after culling).
		[[nodiscard]] std::size_t ItemCount() const noexcept { return entries.size(); }
	};
} // namespace engine::graphics
