#include <catch2/catch_test_macros.hpp>

#include "ecs/hierarchy.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "graphics/sprite.hpp"
#include "graphics/transform.hpp"
#include "ui/ninepatch.hpp"
#include "ui/uisystems.hpp"

namespace
{
	using engine::ui::NinePatch;
	using engine::ui::NinePatchLayout;

	// A 12 x 12 picture at (100, 50) with borders of 3, 3, 4 and 2 (left, right, top, bottom).
	NinePatch Patch(int width, int height)
	{
		return NinePatch{.source = {.x = 100, .y = 50, .width = 12, .height = 12},
		                 .left = 3,
		                 .right = 3,
		                 .top = 4,
		                 .bottom = 2,
		                 .width = width,
		                 .height = height};
	}
} // namespace

TEST_CASE("Nine-patch corners keep their size and the middle stretches", "[ui][ninepatch]")
{
	const auto pieces = NinePatchLayout(Patch(40, 30));
	// Top-left corner, top edge, top-right corner.
	CHECK(pieces[0].width == 3);
	CHECK(pieces[0].height == 4);
	CHECK(pieces[0].offset.x == 0.0f);
	CHECK(pieces[1].width == 34);
	CHECK(pieces[1].offset.x == 3.0f);
	CHECK(pieces[2].width == 3);
	CHECK(pieces[2].offset.x == 37.0f);
	// The middle row is as tall as what is left between the top and bottom borders.
	CHECK(pieces[3].height == 24);
	CHECK(pieces[3].offset.y == 4.0f);
	CHECK(pieces[4].width == 34);
	CHECK(pieces[4].height == 24);
	// Bottom row.
	CHECK(pieces[8].width == 3);
	CHECK(pieces[8].height == 2);
	CHECK(pieces[8].offset.x == 37.0f);
	CHECK(pieces[8].offset.y == 28.0f);
}

TEST_CASE("Nine-patch pieces cut the picture at the borders", "[ui][ninepatch]")
{
	const auto pieces = NinePatchLayout(Patch(40, 30));
	CHECK(pieces[0].source == engine::graphics::PixelRect{.x = 100, .y = 50, .width = 3, .height = 4});
	CHECK(pieces[4].source == engine::graphics::PixelRect{.x = 103, .y = 54, .width = 6, .height = 6});
	CHECK(pieces[8].source == engine::graphics::PixelRect{.x = 109, .y = 60, .width = 3, .height = 2});
}

TEST_CASE("The pieces exactly cover the panel", "[ui][ninepatch]")
{
	for (const int width : {0, 1, 5, 6, 7, 40})
	{
		for (const int height : {0, 3, 6, 7, 30})
		{
			const auto pieces = NinePatchLayout(Patch(width, height));
			int top = 0;
			int left = 0;
			for (int column = 0; column < 3; ++column)
				left += pieces[static_cast<std::size_t>(column)].width;
			for (int row = 0; row < 3; ++row)
				top += pieces[static_cast<std::size_t>(row * 3)].height;
			CHECK(left == width);
			CHECK(top == height);
		}
	}
}

TEST_CASE("A panel smaller than its borders shrinks them in proportion", "[ui][ninepatch]")
{
	const auto pieces = NinePatchLayout(Patch(3, 30)); // borders of 3 + 3 into 3: 1 and 2
	CHECK(pieces[0].width == 1);
	CHECK(pieces[2].width == 2);
	CHECK(pieces[1].width == 0);
	CHECK_FALSE(pieces[1].Visible());
	CHECK(pieces[0].Visible());
}

TEST_CASE("Pieces with no border or no size are not visible", "[ui][ninepatch]")
{
	NinePatch patch = Patch(40, 30);
	patch.left = 0;
	const auto pieces = NinePatchLayout(patch);
	CHECK_FALSE(pieces[0].Visible());
	CHECK_FALSE(pieces[3].Visible());
	CHECK(pieces[1].Visible());

	CHECK_FALSE(NinePatchLayout(Patch(0, 0))[4].Visible());
}

TEST_CASE("The system gives a nine-patch nine child sprites and re-lays them when it resizes", "[ui][ninepatch]")
{
	using engine::ecs::Entity;
	engine::ecs::World world;
	const Entity panel = world.registry.Create();
	world.registry.Add<engine::graphics::Transform>(panel, engine::graphics::Position{10.0f, 20.0f});
	NinePatch patch = Patch(40, 30);
	patch.layer = 7;
	world.registry.Add<NinePatch>(panel, patch);

	engine::ecs::FrameContext context;
	engine::ui::NinePatchSystem(world, context);
	const auto pieces = world.registry.GetComponent<engine::ui::NinePatchPieces>(panel).pieces;
	for (const Entity piece : pieces)
	{
		REQUIRE(world.registry.IsAlive(piece));
		CHECK(world.registry.GetComponent<engine::ecs::Parent>(piece).entity == panel);
	}
	const auto& centre = world.registry.GetComponent<engine::graphics::Sprite>(pieces[4]);
	CHECK(centre.width == 34);
	CHECK(centre.height == 24);
	CHECK(centre.layer == 7);
	CHECK(world.registry.GetComponent<engine::graphics::LocalTransform>(pieces[4]).position.x == 3.0f);

	world.registry.GetComponent<NinePatch>(panel).width = 60;
	world.registry.GetComponent<NinePatch>(panel).left = 0; // the left column goes away
	engine::ui::NinePatchSystem(world, context);
	CHECK(world.registry.GetComponent<engine::ecs::Parent>(pieces[4]).entity == panel);
	CHECK(world.registry.GetComponent<engine::graphics::Sprite>(pieces[4]).width == 57);
	CHECK_FALSE(world.registry.HasComponent<engine::graphics::Sprite>(pieces[0]));
	CHECK(world.registry.GetComponent<engine::ui::NinePatchPieces>(panel).pieces == pieces); // no new ones
}
