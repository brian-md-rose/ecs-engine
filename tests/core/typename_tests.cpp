#include <catch2/catch_test_macros.hpp>

#include "core/typename.hpp"
#include "graphics/transform.hpp"

namespace
{
	struct Local
	{
	};
	enum class Mood
	{
		Calm
	};
} // namespace

TEST_CASE("TypeName gives a type's name without its namespaces", "[core][typename]")
{
	CHECK(engine::core::TypeName<engine::graphics::Transform>() == "Transform");
	CHECK(engine::core::QualifiedTypeName<engine::graphics::Transform>() == "engine::graphics::Transform");
	CHECK(engine::core::TypeName<Local>() == "Local");
	CHECK(engine::core::TypeName<Mood>() == "Mood");
	CHECK(engine::core::TypeName<int>() == "int");
}

static_assert(engine::core::TypeName<engine::graphics::Transform>() == "Transform");
