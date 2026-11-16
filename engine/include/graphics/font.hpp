#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/color.hpp"
#include "graphics/image.hpp"
#include "graphics/transform.hpp"

namespace engine::graphics
{
	// A fixed-width bitmap font: one picture per character, cut from the atlas the Renderer2D
	// draws with. Every character moves the pen the same distance, so text lines up in columns.
	// Characters without a glyph (space, for one) are drawn as gaps.
	class BitmapFont
	{
		std::array<std::optional<PixelRect>, 128> glyphs{};

	public:
		int advance = 6;    // pixels from one character to the next
		int lineHeight = 8; // pixels from one line to the next

		// Where character's picture is in the atlas. Only 7-bit ASCII; others are ignored.
		void SetGlyph(char character, PixelRect source);

		// nullptr if character has no picture.
		[[nodiscard]] const PixelRect* Glyph(char character) const noexcept;
	};

	// One character to draw: where its top-left goes and which part of the atlas it shows.
	struct GlyphQuad
	{
		Position screen{};
		PixelRect source{};
	};

	// Lays text out from origin, its top-left, one quad per character with a glyph, each scale
	// times its size. '\n' starts a new line. Appends to quads, so a caller can reuse the vector.
	// Pure: the tests check it without a renderer.
	void LayoutText(const BitmapFont& font, std::string_view text, Position origin, int scale,
	                std::vector<GlyphQuad>& quads);

	// The size text takes up when laid out at scale: the widest line, and every line.
	[[nodiscard]] int TextWidth(const BitmapFont& font, std::string_view text, int scale = 1);
	[[nodiscard]] int TextHeight(const BitmapFont& font, std::string_view text, int scale = 1);

	// Component: text drawn with the Renderer2D's font, its top-left at the entity's Transform.
	// The glyphs are white in the atlas and take on color.
	struct Text
	{
		std::string value{};
		Color color{};
		std::uint8_t layer = 0; // as Shape::layer
		int scale = 1;          // whole numbers only, so the pixels stay square and sharp
	};
} // namespace engine::graphics
