#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"
#include "graphics/camera.hpp"
#include "graphics/clip.hpp"
#include "graphics/drawbatch.hpp"
#include "graphics/screenspace.hpp"
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

TEST_CASE("Text wraps to its wrapWidth and aligns each line against the entity", "[graphics][batch]")
{
	BitmapFont font;
	font.SetGlyph('H', {.x = 30, .y = 0, .width = 5, .height = 7});
	font.SetGlyph('I', {.x = 36, .y = 0, .width = 5, .height = 7});

	Registry registry;
	const Entity entity = registry.Create();
	registry.Add<Transform>(entity, Position{100.0f, 4.0f});
	// Two characters fit in 12 pixels: "HI" and "H" on separate lines, right-aligned at x 100.
	registry.Add<Text>(entity, Text{.value = "HI H", .wrapWidth = 12, .align = engine::graphics::Align::Right});

	DrawBatch batch;
	batch.Build(registry, AtlasSettings(&font));
	REQUIRE(batch.ItemCount() == 3);
	const auto vertices = batch.Vertices();
	CHECK(vertices[0].position.x == 88.0f); // first line, 12 wide, ends at 100
	CHECK(vertices[4].position.x == 94.0f);
	CHECK(vertices[8].position.x == 94.0f); // second line, 6 wide
	CHECK(vertices[8].position.y == 12.0f); // one line height (8) lower
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

namespace
{
	constexpr Camera kZoomCamera{.logicalWidth = 100, .logicalHeight = 100, .position = {10.0f, 10.0f}, .zoom = 2};
} // namespace

TEST_CASE("Zoom scales world-space shapes and moves them about the camera", "[graphics][batch][camera]")
{
	Registry registry;
	AddRectangle(registry, {20.0f, 15.0f}, 0); // 8 x 4
	DrawBatch batch;
	batch.Build(registry, kZoomCamera);
	REQUIRE(batch.ItemCount() == 1);
	const auto vertices = batch.Vertices();
	CHECK(vertices[0].position.x == 20.0f); // (20 - 10) * 2
	CHECK(vertices[0].position.y == 10.0f); // (15 - 10) * 2
	CHECK(vertices[2].position.x == 36.0f); // 8 wide, doubled
	CHECK(vertices[2].position.y == 18.0f); // 4 high, doubled
}

TEST_CASE("Culling uses the zoomed size and the zoomed view", "[graphics][batch][camera]")
{
	Registry registry;
	AddRectangle(registry, {59.0f, 20.0f}, 0); // screen x 98: just inside the 100-wide view
	AddRectangle(registry, {60.0f, 20.0f}, 0); // screen x 100: outside
	AddRectangle(registry, {5.0f, 20.0f}, 0);  // screen x -10, 16 wide: reaches x 6, inside
	AddRectangle(registry, {2.0f, 20.0f}, 0);  // screen x -16, ends at 0: outside
	DrawBatch batch;
	batch.Build(registry, kZoomCamera);
	CHECK(batch.ItemCount() == 2);
}

TEST_CASE("Zoom scales sprites and text, and the text's wrap width", "[graphics][batch][camera]")
{
	BitmapFont font;
	font.SetGlyph('H', {.x = 30, .y = 0, .width = 5, .height = 7});

	Registry registry;
	const Entity sprite = AddSprite(registry, {12.0f, 12.0f}, 0);
	(void)sprite;
	const Entity text = registry.Create();
	registry.Add<Transform>(text, Position{20.0f, 20.0f});
	// 12 world pixels hold two 6-pixel characters, so "HHH" wraps after two.
	registry.Add<engine::graphics::Text>(text, engine::graphics::Text{.value = "HHH", .wrapWidth = 12});

	auto settings = AtlasSettings(&font);
	settings.camera = kZoomCamera;
	DrawBatch batch;
	batch.Build(registry, settings);
	REQUIRE(batch.ItemCount() == 4);
	const auto vertices = batch.Vertices();
	// The sprite (8 x 4 in the atlas) is drawn at (12 - 10) * 2 = 4, twice as big.
	CHECK(vertices[0].position.x == 4.0f);
	CHECK(vertices[2].position.x == 20.0f);
	CHECK(vertices[2].position.y == 12.0f);
	// Text: first glyph at screen 20, the second one character (6 * 2) along, the third on the next line.
	CHECK(vertices[4].position.x == 20.0f);
	CHECK(vertices[8].position.x == 32.0f);
	CHECK(vertices[12].position.x == 20.0f);
	CHECK(vertices[12].position.y == 36.0f); // 20 + one line of 8 pixels, doubled
}

TEST_CASE("Screen-space entities ignore the camera, and so do their children", "[graphics][batch][camera]")
{
	using engine::ecs::Parent;
	using engine::graphics::ScreenSpace;

	Registry registry;
	const Entity world = AddRectangle(registry, {30.0f, 30.0f}, 0);
	const Entity hud = AddRectangle(registry, {30.0f, 30.0f}, 0);
	registry.Add<ScreenSpace>(hud);
	const Entity panel = registry.Create();
	registry.Add<ScreenSpace>(panel);
	registry.Add<Transform>(panel, Position{0.0f, 0.0f});
	const Entity child = AddRectangle(registry, {50.0f, 40.0f}, 0);
	registry.Add<Parent>(child, Parent{.entity = panel});
	const Entity grandchild = AddRectangle(registry, {60.0f, 40.0f}, 0);
	registry.Add<Parent>(grandchild, Parent{.entity = child});
	(void)world;

	DrawBatch batch;
	batch.Build(registry, kZoomCamera);
	REQUIRE(batch.ItemCount() == 4);
	const auto vertices = batch.Vertices();
	// Entries come out in creation order here (same layer): world, hud, child, grandchild.
	CHECK(vertices[0].position.x == 40.0f); // world: (30 - 10) * 2
	CHECK(vertices[4].position.x == 30.0f); // screen-space: as is
	CHECK(vertices[6].position.x == 38.0f); // ... and 8 wide, not 16
	CHECK(vertices[8].position.x == 50.0f); // child of a screen-space panel
	CHECK(vertices[12].position.x == 60.0f);
}

TEST_CASE("The camera offset moves the world but not screen-space entities", "[graphics][batch][camera]")
{
	Registry registry;
	AddRectangle(registry, {10.0f, 10.0f}, 0);
	const Entity hud = AddRectangle(registry, {10.0f, 10.0f}, 0);
	registry.Add<engine::graphics::ScreenSpace>(hud);

	Camera camera = kCamera;
	camera.offset = {3.0f, -2.0f};
	DrawBatch batch;
	batch.Build(registry, camera);
	REQUIRE(batch.ItemCount() == 2);
	CHECK(batch.Vertices()[0].position.x == 13.0f);
	CHECK(batch.Vertices()[0].position.y == 8.0f);
	CHECK(batch.Vertices()[4].position.x == 10.0f);
}

TEST_CASE("A sprite with a width and height is stretched to it", "[graphics][batch][sprite]")
{
	Registry registry;
	const Entity entity = AddSprite(registry, {10.0f, 20.0f}, 0); // source 8 x 4
	registry.GetComponent<Sprite>(entity).width = 30;
	registry.GetComponent<Sprite>(entity).height = 6;

	DrawBatch batch;
	batch.Build(registry, AtlasSettings());
	REQUIRE(batch.ItemCount() == 1);
	const auto vertices = batch.Vertices();
	CHECK(vertices[2].position.x == 40.0f);
	CHECK(vertices[2].position.y == 26.0f);
	// The whole source is still sampled: 16..24 across a 64-wide atlas, 8..12 down a 32-high one.
	CHECK(vertices[0].uv.x == Approx(16.0f / 64.0f));
	CHECK(vertices[2].uv.x == Approx(24.0f / 64.0f));
	CHECK(vertices[2].uv.y == Approx(12.0f / 32.0f));

	// Only one axis set: the other keeps the picture's size.
	registry.GetComponent<Sprite>(entity).height = 0;
	batch.Build(registry, AtlasSettings());
	CHECK(batch.Vertices()[2].position.x == 40.0f);
	CHECK(batch.Vertices()[2].position.y == 24.0f);
}

TEST_CASE("A stretched sprite is zoomed with the world", "[graphics][batch][sprite][camera]")
{
	Registry registry;
	const Entity entity = AddSprite(registry, {12.0f, 12.0f}, 0);
	registry.GetComponent<Sprite>(entity).width = 10;
	registry.GetComponent<Sprite>(entity).height = 5;

	auto settings = AtlasSettings();
	settings.camera = kZoomCamera; // zoom 2, position (10, 10)
	DrawBatch batch;
	batch.Build(registry, settings);
	REQUIRE(batch.ItemCount() == 1);
	CHECK(batch.Vertices()[0].position.x == 4.0f);
	CHECK(batch.Vertices()[2].position.x == 24.0f);
	CHECK(batch.Vertices()[2].position.y == 14.0f);
}

namespace
{
	using engine::graphics::ClipRect;

	void AddClip(Registry& registry, Entity entity, PixelRect rect)
	{
		registry.Add<ClipRect>(entity, ClipRect{.rect = rect});
	}
} // namespace

TEST_CASE("Without a clip everything is one run", "[graphics][batch][clip]")
{
	Registry registry;
	AddRectangle(registry, {0.0f, 0.0f}, 0);
	AddRectangle(registry, {20.0f, 0.0f}, 1);
	DrawBatch batch;
	batch.Build(registry, kCamera);
	REQUIRE(batch.Runs().size() == 1);
	CHECK(batch.Runs()[0].firstIndex == 0);
	CHECK(batch.Runs()[0].indexCount == 12);
	CHECK_FALSE(batch.Runs()[0].clipped);

	Registry empty;
	batch.Build(empty, kCamera);
	CHECK(batch.Runs().empty());
}

TEST_CASE("Entries with the same clip share a run and different clips split it", "[graphics][batch][clip]")
{
	const PixelRect a{.x = 0, .y = 0, .width = 100, .height = 100};
	const PixelRect b{.x = 50, .y = 50, .width = 100, .height = 100};
	Registry registry;
	AddClip(registry, AddRectangle(registry, {10.0f, 10.0f}, 0), a);
	AddClip(registry, AddRectangle(registry, {20.0f, 10.0f}, 0), a);
	AddClip(registry, AddRectangle(registry, {60.0f, 60.0f}, 0), b);
	AddRectangle(registry, {70.0f, 70.0f}, 0);

	DrawBatch batch;
	batch.Build(registry, kCamera);
	REQUIRE(batch.ItemCount() == 4);
	REQUIRE(batch.Runs().size() == 3);
	CHECK(batch.Runs()[0].clipped);
	CHECK(batch.Runs()[0].clip == a);
	CHECK(batch.Runs()[0].firstIndex == 0);
	CHECK(batch.Runs()[0].indexCount == 12);
	CHECK(batch.Runs()[1].clip == b);
	CHECK(batch.Runs()[1].firstIndex == 12);
	CHECK(batch.Runs()[1].indexCount == 6);
	CHECK_FALSE(batch.Runs()[2].clipped);
	CHECK(batch.Runs()[2].firstIndex == 18);
	CHECK(batch.Runs()[2].indexCount == 6);
}

TEST_CASE("Runs follow the layer order, so a clip never reorders what is drawn over it", "[graphics][batch][clip]")
{
	const PixelRect a{.x = 0, .y = 0, .width = 100, .height = 100};
	Registry registry;
	AddClip(registry, AddRectangle(registry, {10.0f, 10.0f}, 1), a);
	AddRectangle(registry, {20.0f, 10.0f}, 2); // unclipped, over the clipped one
	AddClip(registry, AddRectangle(registry, {30.0f, 10.0f}, 3), a);

	DrawBatch batch;
	batch.Build(registry, kCamera);
	REQUIRE(batch.Runs().size() == 3);
	CHECK(batch.Runs()[0].clipped);
	CHECK_FALSE(batch.Runs()[1].clipped);
	CHECK(batch.Runs()[2].clipped);
	int total = 0;
	for (const auto& run : batch.Runs())
	{
		CHECK(run.firstIndex == total);
		total += run.indexCount;
	}
	CHECK(total == static_cast<int>(batch.Indices().size()));
}

TEST_CASE("A clip applies to the entity's descendants and nested clips intersect", "[graphics][batch][clip]")
{
	using engine::ecs::Parent;
	const PixelRect panel{.x = 10, .y = 10, .width = 100, .height = 100};
	const PixelRect list{.x = 60, .y = 0, .width = 100, .height = 50};

	Registry registry;
	const Entity root = registry.Create();
	registry.Add<Transform>(root, Position{0.0f, 0.0f});
	AddClip(registry, root, panel);
	const Entity child = AddRectangle(registry, {70.0f, 20.0f}, 0);
	registry.Add<Parent>(child, Parent{.entity = root});
	const Entity inner = AddRectangle(registry, {80.0f, 20.0f}, 0);
	registry.Add<Parent>(inner, Parent{.entity = child});
	AddClip(registry, inner, list);

	DrawBatch batch;
	batch.Build(registry, kCamera);
	REQUIRE(batch.Runs().size() == 2);
	CHECK(batch.Runs()[0].clip == panel);
	// Overlap of (10, 10, 100 x 100) and (60, 0, 100 x 50): x 60 to 110, y 10 to 50.
	CHECK(batch.Runs()[1].clip == PixelRect{.x = 60, .y = 10, .width = 50, .height = 40});
}

TEST_CASE("Things wholly outside their clip are culled", "[graphics][batch][clip]")
{
	const PixelRect rect{.x = 10, .y = 10, .width = 20, .height = 20};
	Registry registry;
	AddClip(registry, AddRectangle(registry, {25.0f, 15.0f}, 0), rect); // 8 x 4 from x 25: sticks out, shown
	AddClip(registry, AddRectangle(registry, {30.0f, 15.0f}, 0), rect); // starts at the edge: culled
	AddClip(registry, AddRectangle(registry, {12.0f, 30.0f}, 0), rect); // starts below the bottom edge: culled
	AddClip(registry, AddRectangle(registry, {0.0f, 12.0f}, 0), rect);  // ends at x 8: culled
	AddRectangle(registry, {100.0f, 100.0f}, 0);                        // no clip: shown

	DrawBatch batch;
	batch.Build(registry, kCamera);
	CHECK(batch.ItemCount() == 2);
}

TEST_CASE("Clipped children of disjoint clips draw nothing", "[graphics][batch][clip]")
{
	using engine::ecs::Parent;
	Registry registry;
	const Entity root = registry.Create();
	registry.Add<Transform>(root, Position{});
	AddClip(registry, root, {.x = 0, .y = 0, .width = 10, .height = 10});
	const Entity child = AddRectangle(registry, {50.0f, 50.0f}, 0);
	registry.Add<Parent>(child, Parent{.entity = root});
	AddClip(registry, child, {.x = 40, .y = 40, .width = 30, .height = 30});

	DrawBatch batch;
	batch.Build(registry, kCamera);
	CHECK(batch.ItemCount() == 0);
	CHECK(batch.Runs().empty());
}

TEST_CASE("A clip is in screen pixels: the camera and zoom don't move it", "[graphics][batch][clip][camera]")
{
	Registry registry;
	// World (20, 15) is screen (20, 10) at the zoomed camera; its 16 x 8 box reaches x 36.
	AddClip(registry, AddRectangle(registry, {20.0f, 15.0f}, 0), {.x = 0, .y = 0, .width = 30, .height = 30});
	// World (40, 15) is screen (60, 10): outside the same clip.
	AddClip(registry, AddRectangle(registry, {40.0f, 15.0f}, 0), {.x = 0, .y = 0, .width = 30, .height = 30});

	DrawBatch batch;
	batch.Build(registry, kZoomCamera);
	CHECK(batch.ItemCount() == 1);
}

TEST_CASE("Clipped text is split into the clip's run, glyph by glyph", "[graphics][batch][clip]")
{
	BitmapFont font;
	font.SetGlyph('H', {.x = 30, .y = 0, .width = 5, .height = 7});

	Registry registry;
	const Entity text = registry.Create();
	registry.Add<Transform>(text, Position{0.0f, 0.0f});
	registry.Add<engine::graphics::Text>(text, engine::graphics::Text{.value = "HHHH"});
	AddClip(registry, text, {.x = 0, .y = 0, .width = 13, .height = 8}); // advance 6: glyphs at 0, 6, 12, 18

	DrawBatch batch;
	batch.Build(registry, AtlasSettings(&font));
	CHECK(batch.ItemCount() == 3); // the one at 18 is wholly outside
	REQUIRE(batch.Runs().size() == 1);
	CHECK(batch.Runs()[0].indexCount == 18);
}

TEST_CASE("A rotated rectangle swings clockwise about its top-left corner", "[graphics][batch]")
{
	Registry registry;
	const Entity entity = AddRectangle(registry, {50.0f, 60.0f}, 0);
	registry.GetComponent<Transform>(entity).rotation = 90.0f;

	DrawBatch batch;
	batch.Build(registry, kCamera);

	REQUIRE(batch.Vertices().size() == 4);
	const auto vertices = batch.Vertices();
	// The 8 x 4 box: its corner stays, its top edge now points down, its left edge points left.
	CHECK(vertices[0].position.x == Approx(50.0f));
	CHECK(vertices[0].position.y == Approx(60.0f));
	CHECK(vertices[1].position.x == Approx(50.0f));
	CHECK(vertices[1].position.y == Approx(68.0f));
	CHECK(vertices[2].position.x == Approx(46.0f));
	CHECK(vertices[2].position.y == Approx(68.0f));
	CHECK(vertices[3].position.x == Approx(46.0f));
	CHECK(vertices[3].position.y == Approx(60.0f));
}

TEST_CASE("A scaled rectangle stretches from its top-left corner, and a negative scale mirrors it", "[graphics][batch]")
{
	Registry registry;
	const Entity entity = AddRectangle(registry, {50.0f, 60.0f}, 0);
	registry.GetComponent<Transform>(entity).scale = {2.0f, 3.0f};

	DrawBatch batch;
	batch.Build(registry, kCamera);
	REQUIRE(batch.Vertices().size() == 4);
	CHECK(batch.Vertices()[0].position.x == 50.0f);
	CHECK(batch.Vertices()[2].position.x == 66.0f);
	CHECK(batch.Vertices()[2].position.y == 72.0f);

	registry.GetComponent<Transform>(entity).scale = {-1.0f, 1.0f};
	batch.Build(registry, kCamera);
	REQUIRE(batch.Vertices().size() == 4);
	CHECK(batch.Vertices()[1].position.x == 42.0f); // the right edge is now on the left
}

TEST_CASE("A circle is stretched into an ellipse by its scale", "[graphics][batch]")
{
	Registry registry;
	const Entity entity = registry.Create();
	registry.Add<Transform>(entity, Transform{.position = {100.0f, 100.0f}, .scale = {2.0f, 1.0f}});
	registry.Add<Shape>(entity, Shape{.kind = ShapeKind::Circle, .radius = 4.0f});

	DrawBatch batch;
	batch.Build(registry, kCamera);
	REQUIRE(batch.Vertices().size() == kCircleVertices);
	CHECK(batch.Vertices()[0].position.x == Approx(108.0f)); // the centre: (4, 4) stretched to (8, 4)
	CHECK(batch.Vertices()[0].position.y == Approx(104.0f));
	CHECK(batch.Vertices()[1].position.x == Approx(116.0f)); // rim at angle 0: 8 + 4 * 2
}

TEST_CASE("A rotated shape is kept while any of it is in view", "[graphics][batch]")
{
	Registry registry;
	// Wholly above the screen unturned, but turned a quarter about its corner it hangs down into view.
	const Entity entity = registry.Create();
	registry.Add<Transform>(entity, Transform{.position = {10.0f, -20.0f}, .rotation = 90.0f});
	registry.Add<Shape>(entity, Shape{.width = 30.0f, .height = 4.0f});
	DrawBatch batch;
	batch.Build(registry, kCamera);
	CHECK(batch.ItemCount() == 1);

	registry.GetComponent<Transform>(entity).rotation = 270.0f;
	batch.Build(registry, kCamera);
	CHECK(batch.ItemCount() == 0);
}
