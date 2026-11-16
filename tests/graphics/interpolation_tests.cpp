#include <catch2/catch_test_macros.hpp>

#include "ecs/component.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/camera.hpp"
#include "graphics/drawbatch.hpp"
#include "graphics/interpolation.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::Registry;
	using engine::graphics::InterpolatedPosition;
	using engine::graphics::Interpolation;
	using engine::graphics::Position;
	using engine::graphics::Transform;
} // namespace

static_assert(engine::ecs::Component<Interpolation>);

TEST_CASE("An interpolated position lies between the previous and the current one", "[graphics][interpolation]")
{
	const Transform now{.position = {20.0f, 10.0f}};
	const Interpolation was{.previous = {10.0f, 10.0f}, .hasPrevious = true};

	CHECK(InterpolatedPosition(now, was, 0.0f).x == 10.0f);
	CHECK(InterpolatedPosition(now, was, 0.25f).x == 12.5f);
	CHECK(InterpolatedPosition(now, was, 1.0f).x == 20.0f);
	CHECK(InterpolatedPosition(now, was, 0.25f).y == 10.0f);
	CHECK(InterpolatedPosition(now, was, 2.0f).x == 20.0f); // alpha is clamped to 0-1
}

TEST_CASE("Without a recorded previous position, the current one is drawn", "[graphics][interpolation]")
{
	const Transform now{.position = {20.0f, 10.0f}};
	CHECK(InterpolatedPosition(now, Interpolation{}, 0.0f).x == 20.0f);
}

TEST_CASE("A move longer than the snap distance is drawn at once", "[graphics][interpolation]")
{
	const Transform now{.position = {200.0f, 10.0f}};
	const Interpolation wrapped{.previous = {0.0f, 10.0f}, .hasPrevious = true, .snapDistance = 8.0f};
	CHECK(InterpolatedPosition(now, wrapped, 0.5f).x == 200.0f);

	const Interpolation step{.previous = {194.0f, 10.0f}, .hasPrevious = true, .snapDistance = 8.0f};
	CHECK(InterpolatedPosition(now, step, 0.5f).x == 197.0f);
}

TEST_CASE("RecordPreviousPositions copies each Transform into its Interpolation", "[graphics][interpolation]")
{
	Registry registry;
	const Entity mover = registry.Create();
	registry.Add<Transform>(mover, Position{3.0f, 4.0f});
	registry.Add<Interpolation>(mover);

	engine::graphics::RecordPreviousPositions(registry);

	const Interpolation& recorded = registry.GetComponent<Interpolation>(mover);
	CHECK(recorded.hasPrevious);
	CHECK(recorded.previous.x == 3.0f);
	CHECK(recorded.previous.y == 4.0f);
}

TEST_CASE("DrawBatch draws Interpolation entities by alpha and the rest where they are", "[graphics][interpolation]")
{
	Registry registry;
	const engine::graphics::Shape square{.width = 4.0f, .height = 4.0f};
	const Entity smooth = registry.Create();
	registry.Add<Transform>(smooth, Position{20.0f, 0.0f});
	registry.Add<engine::graphics::Shape>(smooth, square);
	registry.Add<Interpolation>(smooth, Interpolation{.previous = {10.0f, 0.0f}, .hasPrevious = true});
	const Entity still = registry.Create();
	registry.Add<Transform>(still, Position{50.0f, 0.0f});
	registry.Add<engine::graphics::Shape>(still, square);

	engine::graphics::DrawBatch batch;
	batch.Build(registry,
	            engine::graphics::BatchSettings{.camera = {.logicalWidth = 224, .logicalHeight = 288}, .alpha = 0.5f});

	REQUIRE(batch.Vertices().size() == 8);
	CHECK(batch.Vertices()[0].position.x == 15.0f); // the first corner of each rectangle
	CHECK(batch.Vertices()[4].position.x == 50.0f);
}
