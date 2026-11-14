#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/paths.hpp"

TEST_CASE("ExecutableDirectory is the folder holding the test executable", "[core][paths]")
{
	const std::filesystem::path directory = engine::core::ExecutableDirectory();
	REQUIRE_FALSE(directory.empty());
	CHECK(std::filesystem::is_directory(directory));
	CHECK(directory.is_absolute());
}
