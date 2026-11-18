#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "core/assert.hpp"
#include "core/log.hpp"

using namespace engine::core;
using engine::SetAssertHandler;

namespace
{
	std::string ReadAll(const std::filesystem::path& file)
	{
		std::ifstream stream(file, std::ios::binary);
		std::ostringstream text;
		text << stream.rdbuf();
		return text.str();
	}

	std::filesystem::path FreshFolder(const char* name)
	{
		const std::filesystem::path folder = std::filesystem::temp_directory_path() / "ecs-engine-log-tests" / name;
		std::error_code error;
		std::filesystem::remove_all(folder, error);
		return folder;
	}

	bool Contains(const std::string& text, const char* part)
	{
		return text.find(part) != std::string::npos;
	}
} // namespace

TEST_CASE("Messages are written to the log file with a level", "[log]")
{
	const std::filesystem::path file = FreshFolder("levels") / "log.txt"; // the folder is made for us
	{
		const LogSession session({.file = file, .minimumLevel = LogLevel::Info, .console = false});
		LogDebug("too quiet");
		LogInfo("hello");
		LogWarning("careful");
		LogError("broken");
	}
	const std::string text = ReadAll(file);
	CHECK(Contains(text, "[INFO ] hello\n"));
	CHECK(Contains(text, "[WARN ] careful\n"));
	CHECK(Contains(text, "[ERROR] broken\n"));
	CHECK_FALSE(Contains(text, "too quiet"));
}

TEST_CASE("The previous log is kept as name.old.ext", "[log]")
{
	const std::filesystem::path file = FreshFolder("rotate") / "log.txt";
	{
		const LogSession first({.file = file, .console = false});
		LogInfo("first run");
	}
	{
		const LogSession second({.file = file, .console = false});
		LogInfo("second run");
	}
	CHECK(Contains(ReadAll(file), "second run"));
	CHECK_FALSE(Contains(ReadAll(file), "first run"));
	CHECK(Contains(ReadAll(file.parent_path() / "log.old.txt"), "first run"));
}

TEST_CASE("A log file that can't be opened doesn't stop the session", "[log]")
{
	const std::filesystem::path folder = FreshFolder("unopenable");
	std::filesystem::create_directories(folder);
	const std::filesystem::path blocker = folder / "blocker";
	std::ofstream(blocker) << "a file, so nothing can be made inside it";
	const std::filesystem::path file = blocker / "log.txt";

	const LogSession session({.file = file, .console = false});
	LogError("falls back to stderr"); // must not crash

	std::error_code error;
	CHECK_FALSE(std::filesystem::exists(file, error));
}

TEST_CASE("Messages below the minimum level are dropped", "[log]")
{
	const std::filesystem::path file = FreshFolder("minimum") / "log.txt";
	{
		const LogSession session({.file = file, .minimumLevel = LogLevel::Warning, .console = false});
		LogInfo("dropped");
		LogWarning("kept");
	}
	const std::string text = ReadAll(file);
	CHECK_FALSE(Contains(text, "dropped"));
	CHECK(Contains(text, "[WARN ] kept\n"));
}

TEST_CASE("A second session leaves the first one alone", "[log]")
{
	const std::filesystem::path file = FreshFolder("overlap") / "log.txt";
	{
		const LogSession first({.file = file, .console = false});
		LogInfo("before");
		{
			const auto previous = SetAssertHandler([](const char*, const char*, const char*, int) {});
			const LogSession second({.file = file.parent_path() / "other.txt", .console = false});
			SetAssertHandler(previous);
		}
		LogInfo("after");
	}
	const std::string text = ReadAll(file);
	CHECK(Contains(text, "before"));
	CHECK(Contains(text, "after"));
}

TEST_CASE("LogIsActive is true only while a LogSession exists", "[log]")
{
	CHECK_FALSE(engine::core::LogIsActive());
	{
		const LogSession session({.file = FreshFolder("active") / "log.txt", .console = false});
		CHECK(engine::core::LogIsActive());
	}
	CHECK_FALSE(engine::core::LogIsActive());
}
