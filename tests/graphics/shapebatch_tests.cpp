#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/camera.hpp"
#include "graphics/shapebatch.hpp"

namespace
{
	using Catch::Approx;
	using engine::ecs::Entity;
	using engine::ecs::Registry;
	using engine::graphics::Camera;
	using engine::graphics::Color;
	using engine::graphics::Position;
	using engine::graphics::Shape;
	using engine::graphics::ShapeBatch;
	using engine::graphics::ShapeKind;
	using engine::graphics::Transform;

	constexpr Camera kCamera{.logicalWidth = 224, .logicalHeight = 288};
	constexpr std::size_t kCircleVertices = static_cast<std::size_t>(ShapeBatch::kCircleSegments) + 1;
	constexpr std::size_t kCircleIndices = static_cast<std::size_t>(ShapeBatch::kCircleSegments) * 3;

	Entity AddRectangle(Registry& registry, Position position, std::uint8_t layer, Color color = {})
	{
		const Entity entity = registry.Create();
		registry.Add<Transform>(entity, position);
		registry.Add<Shape>(entity, Shape{.width = 8.0f, .height = 4.0f, .color = color, .layer = layer});
		return entity;
	}
} // namespace

TEST_CASE("An empty registry builds an empty batch", "[graphics][batch]")
{
	Registry registry;
	ShapeBatch batch;
	batch.Build(registry, kCamera);

	CHECK(batch.ShapeCount() == 0);
	CHECK(batch.Vertices().empty());
	CHECK(batch.Indices().empty());
}

TEST_CASE("A rectangle becomes two triangles at its corners", "[graphics][batch]")
{
	Registry registry;
	AddRectangle(registry, {10.0f, 20.0f}, 0, {1, 2, 3});

	ShapeBatch batch;
	batch.Build(registry, kCamera);

	REQUIRE(batch.Vertices().size() == 4);
	REQUIRE(batch.Indices().size() == 6);

	const auto vertices = batch.Vertices();
	CHECK(vertices[0].position.x == 10.0f);
	CHECK(vertices[0].position.y == 20.0f);
	CHECK(vertices[2].position.x == 18.0f); // opposite corner
	CHECK(vertices[2].position.y == 24.0f);
	for (const auto& vertex : vertices)
	{
		CHECK(vertex.color.b == 3);
	}
	for (const int index : batch.Indices())
	{
		CHECK(index >= 0);
		CHECK(index < 4);
	}
}

TEST_CASE("A circle becomes a fan around its centre", "[graphics][batch]")
{
	Registry registry;
	const Entity entity = registry.Create();
	registry.Add<Transform>(entity, Position{10.0f, 10.0f});
	registry.Add<Shape>(entity, Shape{.kind = ShapeKind::Circle, .radius = 5.0f});

	ShapeBatch batch;
	batch.Build(registry, kCamera);

	REQUIRE(batch.Vertices().size() == kCircleVertices);
	REQUIRE(batch.Indices().size() == kCircleIndices);

	// The transform is the bounding box's top-left, so the centre is one radius in.
	const auto vertices = batch.Vertices();
	CHECK(vertices[0].position.x == 15.0f);
	CHECK(vertices[0].position.y == 15.0f);
	for (std::size_t i = 1; i < vertices.size(); ++i)
	{
		const float dx = vertices[i].position.x - 15.0f;
		const float dy = vertices[i].position.y - 15.0f;
		CHECK(dx * dx + dy * dy == Approx(25.0f));
	}
}

TEST_CASE("Shapes are batched in ascending layer order", "[graphics][batch]")
{
	Registry registry;
	AddRectangle(registry, {0.0f, 0.0f}, 2, {2, 0, 0});
	AddRectangle(registry, {0.0f, 0.0f}, 0, {0, 0, 0});
	AddRectangle(registry, {0.0f, 0.0f}, 1, {1, 0, 0});

	ShapeBatch batch;
	batch.Build(registry, kCamera);

	REQUIRE(batch.ShapeCount() == 3);
	const auto vertices = batch.Vertices();
	CHECK(vertices[0].color.r == 0);
	CHECK(vertices[4].color.r == 1);
	CHECK(vertices[8].color.r == 2);
}

TEST_CASE("Shapes on the same layer keep the registry's order", "[graphics][batch]")
{
	Registry registry;
	for (std::uint8_t i = 0; i < 5; ++i)
	{
		AddRectangle(registry, {0.0f, 0.0f}, 1, {i, 0, 0});
	}

	ShapeBatch batch;
	batch.Build(registry, kCamera);

	REQUIRE(batch.ShapeCount() == 5);
	for (std::size_t i = 0; i < 5; ++i)
	{
		CHECK(batch.Vertices()[i * 4].color.r == static_cast<std::uint8_t>(i));
	}
}

TEST_CASE("Mixed shapes index their own vertices", "[graphics][batch]")
{
	Registry registry;
	AddRectangle(registry, {0.0f, 0.0f}, 0);
	const Entity circle = registry.Create();
	registry.Add<Transform>(circle, Position{50.0f, 50.0f});
	registry.Add<Shape>(circle, Shape{.kind = ShapeKind::Circle, .radius = 3.0f, .layer = 1});

	ShapeBatch batch;
	batch.Build(registry, kCamera);

	REQUIRE(batch.Vertices().size() == 4 + kCircleVertices);
	REQUIRE(batch.Indices().size() == 6 + kCircleIndices);
	// The circle's triangles all start at its centre, the first vertex after the rectangle.
	CHECK(batch.Indices()[6] == 4);
	for (const int index : batch.Indices())
	{
		CHECK(index < static_cast<int>(batch.Vertices().size()));
	}
}

TEST_CASE("The camera position moves shapes on screen", "[graphics][batch]")
{
	Registry registry;
	AddRectangle(registry, {100.0f, 50.0f}, 0);

	ShapeBatch batch;
	batch.Build(registry, Camera{.logicalWidth = 224, .logicalHeight = 288, .position = {90.0f, 40.0f}});

	REQUIRE(batch.ShapeCount() == 1);
	CHECK(batch.Vertices()[0].position.x == 10.0f);
	CHECK(batch.Vertices()[0].position.y == 10.0f);
}

TEST_CASE("Shapes entirely outside the camera are skipped", "[graphics][batch]")
{
	Registry registry;
	AddRectangle(registry, {-8.0f, 0.0f}, 0);    // ends exactly at the left edge
	AddRectangle(registry, {224.0f, 0.0f}, 0);   // starts exactly at the right edge
	AddRectangle(registry, {0.0f, 300.0f}, 0);   // below the screen
	AddRectangle(registry, {-4.0f, -2.0f}, 0);   // partly on screen: kept
	AddRectangle(registry, {220.0f, 286.0f}, 0); // partly on screen: kept

	ShapeBatch batch;
	batch.Build(registry, kCamera);

	CHECK(batch.ShapeCount() == 2);
}

TEST_CASE("Rebuilding replaces the previous batch", "[graphics][batch]")
{
	Registry registry;
	const Entity entity = AddRectangle(registry, {0.0f, 0.0f}, 0);

	ShapeBatch batch;
	batch.Build(registry, kCamera);
	REQUIRE(batch.ShapeCount() == 1);

	registry.Destroy(entity);
	batch.Build(registry, kCamera);
	CHECK(batch.ShapeCount() == 0);
	CHECK(batch.Vertices().empty());
	CHECK(batch.Indices().empty());
}
