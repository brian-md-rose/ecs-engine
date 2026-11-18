#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
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

TEST_CASE("AlignedX places text against its anchor on whole pixels", "[graphics][font]")
{
	using engine::graphics::Align;
	CHECK(engine::graphics::AlignedX(16.0f, Align::Left, 30) == 16.0f);
	CHECK(engine::graphics::AlignedX(16.5f, Align::Left, 30) == 16.0f);
	CHECK(engine::graphics::AlignedX(112.0f, Align::Centre, 30) == 97.0f);
	CHECK(engine::graphics::AlignedX(112.0f, Align::Centre, 31) == 96.0f);
	CHECK(engine::graphics::AlignedX(208.0f, Align::Right, 30) == 178.0f);
}

namespace
{
	using engine::graphics::Align;
	using engine::graphics::MeasureText;
	using engine::graphics::WrapText;

	// Advance 6: a width of 30 holds 5 characters.
	[[nodiscard]] std::vector<std::string> Wrap(std::string_view text, int maxWidth, int scale = 1)
	{
		std::vector<std::string> lines;
		for (const std::string_view line : WrapText(TestFont(), text, maxWidth, scale))
			lines.emplace_back(line);
		return lines;
	}
} // namespace

TEST_CASE("WrapText breaks at spaces and drops the space it breaks on", "[graphics][font]")
{
	using Lines = std::vector<std::string>;
	CHECK(Wrap("aa bb cc", 30) == Lines{"aa bb", "cc"});
	CHECK(Wrap("aa bb cc", 36) == Lines{"aa bb", "cc"}); // 6 characters: "aa bb " needs 8 for cc
	CHECK(Wrap("aa bb cc", 48) == Lines{"aa bb cc"});    // exactly 8 characters fit
	CHECK(Wrap("aa bb cc", 42) == Lines{"aa bb", "cc"}); // 7 characters: one short
}

TEST_CASE("WrapText counts the width in characters times scale", "[graphics][font]")
{
	using Lines = std::vector<std::string>;
	CHECK(Wrap("aa bb", 24, 2) == Lines{"aa", "bb"}); // 12 pixels a character: 2 fit
	CHECK(Wrap("aa bb", 60, 2) == Lines{"aa bb"});
}

TEST_CASE("WrapText breaks a word longer than the width by characters", "[graphics][font]")
{
	using Lines = std::vector<std::string>;
	CHECK(Wrap("abcdefghijkl", 30) == Lines{"abcde", "fghij", "kl"});
	CHECK(Wrap("xy abcdefghij", 30) == Lines{"xy", "abcde", "fghij"});
	CHECK(Wrap("abcdefg h", 30) == Lines{"abcde", "fg h"}); // the rest of the word starts the next line
	CHECK(Wrap("abc", 1) == Lines{"a", "b", "c"});          // narrower than a character: one per line
}

TEST_CASE("WrapText respects newlines, empty lines and empty text", "[graphics][font]")
{
	using Lines = std::vector<std::string>;
	CHECK(Wrap("", 30).empty());
	CHECK(Wrap("a\nb", 30) == Lines{"a", "b"});
	CHECK(Wrap("a\n\nb", 30) == Lines{"a", "", "b"});
	CHECK(Wrap("a\n", 30) == Lines{"a", ""});
	CHECK(Wrap("\n", 30) == Lines{"", ""});
	CHECK(Wrap("aa bb\ncc dd ee", 30) == Lines{"aa bb", "cc dd", "ee"});
}

TEST_CASE("WrapText with no width only splits at newlines and keeps spaces", "[graphics][font]")
{
	using Lines = std::vector<std::string>;
	CHECK(Wrap("aa bb cc dd ee ff", 0) == Lines{"aa bb cc dd ee ff"});
	CHECK(Wrap("  indented", 0) == Lines{"  indented"});
	CHECK(Wrap("a  b", 0) == Lines{"a  b"});
	CHECK(Wrap("a b", -5) == Lines{"a b"});
}

TEST_CASE("WrapText drops extra spaces at a break but keeps a leading indent", "[graphics][font]")
{
	using Lines = std::vector<std::string>;
	CHECK(Wrap("aa   bb", 30) == Lines{"aa", "bb"});
	CHECK(Wrap(" aa", 30) == Lines{" aa"});
	CHECK(Wrap("aa ", 12) == Lines{"aa"});
}

TEST_CASE("MeasureText gives the wrapped width and height", "[graphics][font]")
{
	const BitmapFont font = TestFont();
	CHECK(MeasureText(font, "", 30).width == 0);
	CHECK(MeasureText(font, "", 30).height == 0);

	const auto wrapped = MeasureText(font, "aa bb cc", 30);
	CHECK(wrapped.width == 5 * 6);
	CHECK(wrapped.height == 2 * 8);

	const auto scaled = MeasureText(font, "aa bb cc", 60, 2);
	CHECK(scaled.width == 5 * 12);
	CHECK(scaled.height == 2 * 16);

	CHECK(MeasureText(font, "abc\nde", 0).width == 18);
	CHECK(MeasureText(font, "abc\nde", 0).height == 16);
}

TEST_CASE("Wrapped text lays out one line below another", "[graphics][font]")
{
	const BitmapFont font = TestFont();
	std::vector<GlyphQuad> quads;
	engine::graphics::LayoutText(font, "AA BB", {10, 20}, 1, 12, Align::Left, quads);
	REQUIRE(quads.size() == 4);
	CHECK(quads[0].screen.x == 10.0f);
	CHECK(quads[0].screen.y == 20.0f);
	CHECK(quads[1].screen.x == 16.0f);
	CHECK(quads[2].screen.x == 10.0f);
	CHECK(quads[2].screen.y == 28.0f);
}

TEST_CASE("Each wrapped line is aligned on its own", "[graphics][font]")
{
	const BitmapFont font = TestFont();
	std::vector<GlyphQuad> quads;
	// "A" is 6 wide, "AAA" 18; centred on x 100: starts at 97 and 91.
	engine::graphics::LayoutText(font, "A\nAAA", {100, 0}, 1, 0, Align::Centre, quads);
	REQUIRE(quads.size() == 4);
	CHECK(quads[0].screen.x == 97.0f);
	CHECK(quads[1].screen.x == 91.0f);

	quads.clear();
	engine::graphics::LayoutText(font, "A\nAAA", {100, 0}, 1, 0, Align::Right, quads);
	REQUIRE(quads.size() == 4);
	CHECK(quads[0].screen.x == 94.0f);
	CHECK(quads[1].screen.x == 82.0f);
}

TEST_CASE("An unknown character draws as the placeholder, a space as a gap", "[graphics][font]")
{
	BitmapFont font = TestFont();
	std::vector<GlyphQuad> quads;
	engine::graphics::LayoutText(font, "A\xC3 B", {0, 0}, 1, quads);
	CHECK(quads.size() == 2); // no placeholder and no '?': the unknown byte and the space are gaps

	font.SetGlyph('?', {.x = 30, .y = 0, .width = 5, .height = 7});
	quads.clear();
	engine::graphics::LayoutText(font, "A\xC3 B", {0, 0}, 1, quads);
	REQUIRE(quads.size() == 3);
	CHECK(quads[1].source.x == 30);
	CHECK(quads[1].screen.x == 6.0f);
	CHECK(quads[2].screen.x == 18.0f);

	font.placeholder = PixelRect{.x = 40, .y = 0, .width = 5, .height = 7};
	quads.clear();
	engine::graphics::LayoutText(font, "z\n\t", {0, 0}, 1, quads); // z is unknown, a tab is a gap
	REQUIRE(quads.size() == 1);
	CHECK(quads[0].source.x == 40);
}

TEST_CASE("WrapText trims trailing spaces on the last line too, but only when wrapping", "[graphics][font]")
{
	using Lines = std::vector<std::string>;
	CHECK(Wrap("aa ", 30) == Lines{"aa"});
	CHECK(Wrap("aa  \nbb", 30) == Lines{"aa", "bb"});
	CHECK(Wrap("aa ", 0) == Lines{"aa "});

	const BitmapFont font = TestFont();
	std::vector<GlyphQuad> quads;
	// "A " centred on 100 starts where "A" would: 97, not 94.
	engine::graphics::LayoutText(font, "A ", {100, 0}, 1, 30, Align::Centre, quads);
	REQUIRE(quads.size() == 1);
	CHECK(quads[0].screen.x == 97.0f);
}
