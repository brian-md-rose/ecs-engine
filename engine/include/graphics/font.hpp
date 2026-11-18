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

		// What to draw for an unknown character, such as one outside 7-bit ASCII or a missing letter, so it
		// shows as a visible mark instead of vanishing. Unset, the picture of '?' is used if the font has one.
		std::optional<PixelRect> placeholder{};

		// The picture to draw for character: its glyph, else the placeholder. nullptr for a space and other
		// control characters, which are gaps, and for an unknown character when there is no placeholder.
		[[nodiscard]] const PixelRect* GlyphOrPlaceholder(char character) const noexcept;
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

	// How a line of text sits against an x position.
	enum class Align : std::uint8_t
	{
		Left,   // anchorX is where the text starts
		Centre, // anchorX is the middle of the text
		Right   // anchorX is where the text ends
	};

	// The size text takes up when laid out at scale: the widest line, and every line.
	[[nodiscard]] int TextWidth(const BitmapFont& font, std::string_view text, int scale = 1);
	[[nodiscard]] int TextHeight(const BitmapFont& font, std::string_view text, int scale = 1);

	// Where text of width pixels (TextWidth) starts so it sits against anchorX the way align
	// says. Whole pixels, so the text stays sharp.
	[[nodiscard]] float AlignedX(float anchorX, Align align, int width) noexcept;

	// Splits text into lines no wider than maxWidth pixels at scale, breaking at spaces. A word longer
	// than the width is broken by characters, and an explicit '\n' always starts a new line (an empty
	// line stays one). The spaces a break falls on are dropped. maxWidth of 0 or less means no wrapping,
	// and a width narrower than one character still gives one character per line. Empty text has no
	// lines. The lines point into text, so it must outlive them.
	[[nodiscard]] std::vector<std::string_view> WrapText(const BitmapFont& font, std::string_view text, int maxWidth,
	                                                     int scale = 1);

	struct TextSize
	{
		int width = 0;  // the widest line, in pixels
		int height = 0; // every line, in pixels
	};

	// The size text takes up once wrapped to maxWidth (0 for no wrapping) and scaled.
	[[nodiscard]] TextSize MeasureText(const BitmapFont& font, std::string_view text, int maxWidth, int scale = 1);

	// As LayoutText, after wrapping to wrapWidth pixels (0 for none). Each line is aligned against
	// origin.x: its start, middle or end. A line's top is origin.y plus its number times the line height.
	void LayoutText(const BitmapFont& font, std::string_view text, Position origin, int scale, int wrapWidth,
	                Align align, std::vector<GlyphQuad>& quads);

	// Component: text drawn with the Renderer2D's font, its top at the entity's Transform (and by default
	// its left; see align).
	// The glyphs are white in the atlas and take on color.
	struct Text
	{
		std::string value{};
		Color color{};
		std::uint8_t layer = 0;    // as Shape::layer
		int scale = 1;             // whole numbers only, so the pixels stay square and sharp
		int wrapWidth = 0;         // pixels; 0 for one line per '\n', else broken at spaces to fit (WrapText)
		Align align = Align::Left; // each line against the entity's x: it starts, is centred, or ends there
	};
} // namespace engine::graphics
