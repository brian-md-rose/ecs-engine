#include <catch2/catch_test_macros.hpp>

#include "ecs/component.hpp"
#include "ecs/registry.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::Registry;
	using engine::graphics::Color;
	using engine::graphics::Position;
	using engine::graphics::Shape;
	using engine::graphics::ShapeKind;
	using engine::graphics::Transform;
} // namespace

static_assert(engine::ecs::Component<Transform>);
static_assert(engine::ecs::Component<Shape>);

TEST_CASE("Shape defaults to an empty white rectangle on layer 0", "[graphics][components]")
{
	const Shape shape;
	CHECK(shape.kind == ShapeKind::Rectangle);
	CHECK(shape.BoundsWidth() == 0.0f);
	CHECK(shape.BoundsHeight() == 0.0f);
	CHECK(shape.color.r == 255);
	CHECK(shape.color.g == 255);
	CHECK(shape.color.b == 255);
	CHECK(shape.layer == 0);
}

TEST_CASE("Shape bounds follow the kind", "[graphics][components]")
{
	const Shape rectangle{.width = 16.0f, .height = 8.0f};
	CHECK(rectangle.BoundsWidth() == 16.0f);
	CHECK(rectangle.BoundsHeight() == 8.0f);

	// A circle's width and height are ignored; its bounds come from the radius.
	const Shape circle{.kind = ShapeKind::Circle, .width = 99.0f, .height = 99.0f, .radius = 4.0f};
	CHECK(circle.BoundsWidth() == 8.0f);
	CHECK(circle.BoundsHeight() == 8.0f);
}

TEST_CASE("Transform and Shape can be stored in a registry", "[graphics][components]")
{
	Registry registry;
	const Entity entity = registry.Create();
	registry.Add<Transform>(entity, Position{10.0f, 20.0f});
	registry.Add<Shape>(entity, Shape{.width = 4.0f, .height = 4.0f, .color = Color{1, 2, 3}, .layer = 2});

	const Transform& transform = registry.GetComponent<Transform>(entity);
	CHECK(transform.position.x == 10.0f);
	CHECK(transform.position.y == 20.0f);

	const Shape& shape = registry.GetComponent<Shape>(entity);
	CHECK(shape.color.b == 3);
	CHECK(shape.layer == 2);
}
