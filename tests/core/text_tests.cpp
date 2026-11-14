#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "core/text.hpp"

namespace
{
	using engine::core::ParseError;
} // namespace

TEST_CASE("SplitLines drops line endings and trailing blank lines", "[core][text]")
{
	const std::vector<std::string_view> lines = engine::core::SplitLines("one\r\ntwo\n\nthree\n\n");
	REQUIRE(lines.size() == 4);
	CHECK(lines[0] == "one");
	CHECK(lines[1] == "two");
	CHECK(lines[2].empty());
	CHECK(lines[3] == "three");
}

TEST_CASE("SplitWords splits on spaces and tabs and remembers columns", "[core][text]")
{
	const auto words = engine::core::SplitWords("  level\t2  7.5");
	REQUIRE(words.size() == 3);
	CHECK(words[0].text == "level");
	CHECK(words[0].column == 3);
	CHECK(words[1].text == "2");
	CHECK(words[1].column == 9);
	CHECK(words[2].text == "7.5");
	CHECK(words[2].column == 12);
}

TEST_CASE("Numbers parse only when the whole word is a number", "[core][text]")
{
	CHECK(engine::core::ParseInt("42") == 42);
	CHECK(engine::core::ParseInt("+7") == 7);
	CHECK(engine::core::ParseInt("-3") == -3);
	CHECK_FALSE(engine::core::ParseInt("4x").has_value());
	CHECK_FALSE(engine::core::ParseInt("").has_value());
	CHECK(engine::core::ParseFloat("7.25") == 7.25f);
	CHECK(engine::core::ParseFloat("3") == 3.0f);
	CHECK_FALSE(engine::core::ParseFloat("1.5s").has_value());
}

TEST_CASE("FormatError for text files writes errors the way compilers do", "[core][text]")
{
	CHECK(engine::core::FormatError("a.txt", ParseError{3, 7, "bad"}) == "a.txt:3:7: bad");
	CHECK(engine::core::FormatError("a.txt", ParseError{3, 0, "bad"}) == "a.txt:3: bad");
	CHECK(engine::core::FormatError("a.txt", ParseError{0, 0, "bad"}) == "a.txt: bad");
}
