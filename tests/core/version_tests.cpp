#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/version.hpp"

TEST_CASE("The version string matches the numeric parts", "[version]")
{
	const std::string expected = std::to_string(engine::kVersionMajor) + "." +
		std::to_string(engine::kVersionMinor) + "." + std::to_string(engine::kVersionPatch);
	CHECK(expected == engine::kVersionString);
}
