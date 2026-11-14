#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "ecs/component.hpp"
#include "graphics/font.hpp"
#include "graphics/sprite.hpp"

namespace
{
	using engine::graphics::BitmapFont;
	using engine::graphics::GlyphQuad;
	using engine::graphics::PixelRect;

	[[nodiscard]] BitmapFont TestFont()
	{
		BitmapFont font;
		font.advance = 6;
		font.lineHeight = 8;
		font.SetGlyph('A', {.x = 10, .y = 0, .width = 5, .height = 7});
		font.SetGlyph('B', {.x = 16, .y = 0, .width = 5, .height = 7});
		return font;
	}
} // namespace

static_assert(engine::ecs::Component<engine::graphics::Text>);
static_assert(engine::ecs::Component<engine::graphics::Sprite>);

TEST_CASE("A font knows only the glyphs it was given", "[graphics][font]")
{
	const BitmapFont font = TestFont();
	REQUIRE(font.Glyph('A') != nullptr);
	CHECK(font.Glyph('A')->x == 10);
	CHECK(font.Glyph('C') == nullptr);
	CHECK(font.Glyph(' ') == nullptr);
	CHECK(font.Glyph(static_cast<char>(200)) == nullptr);
}

TEST_CASE("LayoutText advances a fixed width per character and skips gaps", "[graphics][font]")
{
	std::vector<GlyphQuad> quads;
	engine::graphics::LayoutText(TestFont(), "A B", {10.0f, 20.0f}, 1, quads);

	REQUIRE(quads.size() == 2); // the space has no glyph but still takes room
	CHECK(quads[0].screen.x == 10.0f);
	CHECK(quads[0].screen.y == 20.0f);
	CHECK(quads[0].source.x == 10);
	CHECK(quads[1].screen.x == 22.0f);
	CHECK(quads[1].source.x == 16);
}

TEST_CASE("LayoutText starts a new line at a newline and scales everything", "[graphics][font]")
{
	std::vector<GlyphQuad> quads{GlyphQuad{}}; // appended to, not replaced
	engine::graphics::LayoutText(TestFont(), "AB\nB", {0.0f, 0.0f}, 2, quads);

	REQUIRE(quads.size() == 4);
	CHECK(quads[2].screen.x == 12.0f);
	CHECK(quads[3].screen.x == 0.0f);
	CHECK(quads[3].screen.y == 16.0f);
}

TEST_CASE("Text size counts the widest line and every line", "[graphics][font]")
{
	const BitmapFont font = TestFont();
	CHECK(engine::graphics::TextWidth(font, "") == 0);
	CHECK(engine::graphics::TextWidth(font, "ABC") == 18);
	CHECK(engine::graphics::TextWidth(font, "A\nABCD\nB", 2) == 48);
	CHECK(engine::graphics::TextHeight(font, "") == 0);
	CHECK(engine::graphics::TextHeight(font, "A\nB") == 16);
	CHECK(engine::graphics::TextHeight(font, "A", 3) == 24);
}
