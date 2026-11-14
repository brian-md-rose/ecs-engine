#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/camera.hpp"
#include "graphics/drawbatch.hpp"
#include "graphics/sprite.hpp"

namespace
{
	using Catch::Approx;
	using engine::ecs::Entity;
	using engine::ecs::Registry;
	using engine::graphics::Camera;
	using engine::graphics::Color;
	using engine::graphics::DrawBatch;
	using engine::graphics::Position;
	using engine::graphics::Shape;
	using engine::graphics::ShapeKind;
	using engine::graphics::Transform;

	constexpr Camera kCamera{.logicalWidth = 224, .logicalHeight = 288};
	constexpr std::size_t kCircleVertices = static_cast<std::size_t>(DrawBatch::kCircleSegments) + 1;
	constexpr std::size_t kCircleIndices = static_cast<std::size_t>(DrawBatch::kCircleSegments) * 3;

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
	DrawBatch batch;
	batch.Build(registry, kCamera);

	CHECK(batch.ItemCount() == 0);
	CHECK(batch.Vertices().empty());
	CHECK(batch.Indices().empty());
}

TEST_CASE("A rectangle becomes two triangles at its corners", "[graphics][batch]")
{
	Registry registry;
	AddRectangle(registry, {10.0f, 20.0f}, 0, {1, 2, 3});

	DrawBatch batch;
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

	DrawBatch batch;
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

	DrawBatch batch;
	batch.Build(registry, kCamera);

	REQUIRE(batch.ItemCount() == 3);
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

	DrawBatch batch;
	batch.Build(registry, kCamera);

	REQUIRE(batch.ItemCount() == 5);
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

	DrawBatch batch;
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

	DrawBatch batch;
	batch.Build(registry, Camera{.logicalWidth = 224, .logicalHeight = 288, .position = {90.0f, 40.0f}});

	REQUIRE(batch.ItemCount() == 1);
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

	DrawBatch batch;
	batch.Build(registry, kCamera);

	CHECK(batch.ItemCount() == 2);
}

TEST_CASE("Rebuilding replaces the previous batch", "[graphics][batch]")
{
	Registry registry;
	const Entity entity = AddRectangle(registry, {0.0f, 0.0f}, 0);

	DrawBatch batch;
	batch.Build(registry, kCamera);
	REQUIRE(batch.ItemCount() == 1);

	registry.Destroy(entity);
	batch.Build(registry, kCamera);
	CHECK(batch.ItemCount() == 0);
	CHECK(batch.Vertices().empty());
	CHECK(batch.Indices().empty());
}

namespace
{
	using engine::graphics::BatchSettings;
	using engine::graphics::BitmapFont;
	using engine::graphics::PixelRect;
	using engine::graphics::Sprite;
	using engine::graphics::Text;

	// A 64 x 32 atlas with its solid pixel at (1, 1).
	[[nodiscard]] BatchSettings AtlasSettings(const BitmapFont* font = nullptr)
	{
		return BatchSettings{.camera = kCamera,
		                     .atlasWidth = 64,
		                     .atlasHeight = 32,
		                     .solid = {.x = 1, .y = 1, .width = 1, .height = 1},
		                     .font = font};
	}

	Entity AddSprite(Registry& registry, Position position, std::uint8_t layer, bool flipX = false)
	{
		const Entity entity = registry.Create();
		registry.Add<Transform>(entity, position);
		registry.Add<Sprite>(entity, Sprite{.source = {.x = 16, .y = 8, .width = 8, .height = 4},
		                                    .tint = {10, 20, 30},
		                                    .layer = layer,
		                                    .flipX = flipX});
		return entity;
	}
} // namespace

TEST_CASE("A sprite becomes a quad showing its part of the atlas, on whole pixels", "[graphics][batch]")
{
	Registry registry;
	AddSprite(registry, {10.4f, 19.6f}, 0);

	DrawBatch batch;
	batch.Build(registry, AtlasSettings());

	REQUIRE(batch.ItemCount() == 1);
	REQUIRE(batch.Vertices().size() == 4);
	const auto vertices = batch.Vertices();
	CHECK(vertices[0].position.x == 10.0f);
	CHECK(vertices[0].position.y == 20.0f);
	CHECK(vertices[2].position.x == 18.0f);
	CHECK(vertices[2].position.y == 24.0f);
	CHECK(vertices[0].uv.x == Approx(16.0f / 64.0f));
	CHECK(vertices[0].uv.y == Approx(8.0f / 32.0f));
	CHECK(vertices[2].uv.x == Approx(24.0f / 64.0f));
	CHECK(vertices[2].uv.y == Approx(12.0f / 32.0f));
	CHECK(vertices[0].color.g == 20);
}

TEST_CASE("A flipped sprite swaps its left and right edges in the atlas", "[graphics][batch]")
{
	Registry registry;
	AddSprite(registry, {0.0f, 0.0f}, 0, true);

	DrawBatch batch;
	batch.Build(registry, AtlasSettings());

	REQUIRE(batch.Vertices().size() == 4);
	CHECK(batch.Vertices()[0].uv.x == Approx(24.0f / 64.0f));
	CHECK(batch.Vertices()[1].uv.x == Approx(16.0f / 64.0f));
}

TEST_CASE("Without an atlas only shapes are drawn", "[graphics][batch]")
{
	Registry registry;
	AddSprite(registry, {0.0f, 0.0f}, 0);
	AddRectangle(registry, {0.0f, 0.0f}, 0);

	DrawBatch batch;
	batch.Build(registry, kCamera);
	CHECK(batch.ItemCount() == 1);
}

TEST_CASE("Shapes sample the atlas's solid pixel", "[graphics][batch]")
{
	Registry registry;
	AddRectangle(registry, {0.0f, 0.0f}, 0);

	DrawBatch batch;
	batch.Build(registry, AtlasSettings());
	REQUIRE(batch.Vertices().size() == 4);
	for (const auto& vertex : batch.Vertices())
	{
		CHECK(vertex.uv.x == Approx(1.5f / 64.0f));
		CHECK(vertex.uv.y == Approx(1.5f / 32.0f));
	}
}

TEST_CASE("Text becomes one quad per glyph in the text's colour", "[graphics][batch]")
{
	BitmapFont font;
	font.SetGlyph('H', {.x = 30, .y = 0, .width = 5, .height = 7});
	font.SetGlyph('I', {.x = 36, .y = 0, .width = 5, .height = 7});

	Registry registry;
	const Entity entity = registry.Create();
	registry.Add<Transform>(entity, Position{4.0f, 4.0f});
	registry.Add<Text>(entity, Text{.value = "HI !", .color = {1, 2, 3}, .layer = 5, .scale = 2});

	DrawBatch batch;
	batch.Build(registry, AtlasSettings(&font));
	REQUIRE(batch.ItemCount() == 2); // ' ' and '!' have no glyph
	const auto vertices = batch.Vertices();
	CHECK(vertices[4].position.x == 16.0f); // second glyph: 4 + 6 * 2
	CHECK(vertices[6].position.x == 26.0f); // 5 pixels wide, doubled
	CHECK(vertices[6].position.y == 18.0f);
	CHECK(vertices[0].color.b == 3);

	batch.Build(registry, AtlasSettings()); // no font: no text
	CHECK(batch.ItemCount() == 0);
}

TEST_CASE("Shapes, sprites and text share one layer order", "[graphics][batch]")
{
	Registry registry;
	AddSprite(registry, {0.0f, 0.0f}, 2);
	AddRectangle(registry, {0.0f, 0.0f}, 3, {7, 0, 0});
	AddRectangle(registry, {0.0f, 0.0f}, 1, {5, 0, 0});

	DrawBatch batch;
	batch.Build(registry, AtlasSettings());
	REQUIRE(batch.ItemCount() == 3);
	CHECK(batch.Vertices()[0].color.r == 5);
	CHECK(batch.Vertices()[4].color.r == 10); // the sprite's tint
	CHECK(batch.Vertices()[8].color.r == 7);
}

TEST_CASE("The tint dims layers below the untinted layer only", "[graphics][batch]")
{
	Registry registry;
	AddRectangle(registry, {0.0f, 0.0f}, 0, {200, 100, 50});
	AddRectangle(registry, {0.0f, 0.0f}, 9, {200, 100, 50});

	DrawBatch batch;
	BatchSettings settings{.camera = kCamera, .tint = {128, 128, 128}, .untintedFromLayer = 9};
	batch.Build(registry, settings);
	REQUIRE(batch.ItemCount() == 2);
	CHECK(batch.Vertices()[0].color.r == 100);
	CHECK(batch.Vertices()[0].color.g == 50);
	CHECK(batch.Vertices()[4].color.r == 200);
}
