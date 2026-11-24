#include <catch2/catch_test_macros.hpp>

#include "editor/consolepanel.hpp"

using engine::core::LogLevel;
using engine::core::LogRecord;
using engine::editor::ConsoleFilter;
using engine::editor::ConsoleLine;
using engine::editor::ConsoleMatches;

TEST_CASE("The Console filter checks the level and the text", "[console]")
{
	const LogRecord warning{1, LogLevel::Warning, "2026-11-23 10:11:12.345", "Missing sprite Goblin"};
	ConsoleFilter filter;
	CHECK(ConsoleMatches(warning, filter));

	filter.text = "goblin"; // ignoring case
	CHECK(ConsoleMatches(warning, filter));
	filter.text = "orc";
	CHECK_FALSE(ConsoleMatches(warning, filter));

	filter.text.clear();
	filter.warning = false;
	CHECK_FALSE(ConsoleMatches(warning, filter));
	CHECK(ConsoleMatches({2, LogLevel::Error, "", "x"}, filter));
	filter.error = false;
	CHECK_FALSE(ConsoleMatches({2, LogLevel::Error, "", "x"}, filter));
}

TEST_CASE("A Console line is the time of day, the level and the message", "[console]")
{
	CHECK(ConsoleLine({1, LogLevel::Error, "2026-11-23 10:11:12.345", "boom"}) == "10:11:12.345 [ERROR] boom");
	CHECK(ConsoleLine({1, LogLevel::Info, "-", "hi"}) == "- [INFO] hi");
}

TEST_CASE("A record's source shows as file and line, or nothing without one", "[console]")
{
	LogRecord record{1, LogLevel::Error, "", "x", "game/src/maze.cpp", 42};
	CHECK(engine::editor::ConsoleSource(record) == "game/src/maze.cpp:42");
	record.line = 0;
	CHECK(engine::editor::ConsoleSource(record).empty());
	CHECK(engine::editor::ConsoleSource({2, LogLevel::Info, "", "SDL says", "", 0}).empty());
}

TEST_CASE("File URLs take Windows and POSIX paths and escape what a URL can't hold", "[console]")
{
	using engine::editor::FileUrl;
	CHECK(FileUrl("D:\\Visual Studio Projects\\a#b.cpp") == "file:///D:/Visual%20Studio%20Projects/a%23b.cpp");
	CHECK(FileUrl("/home/me/x.cpp") == "file:///home/me/x.cpp");
	CHECK(FileUrl("\\\\server\\share\\x.cpp") == "file://server/share/x.cpp");
}
