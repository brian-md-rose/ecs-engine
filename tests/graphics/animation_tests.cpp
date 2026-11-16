#include <catch2/catch_test_macros.hpp>

#include "ecs/component.hpp"
#include "ecs/entity.hpp"
#include "ecs/world.hpp"
#include "graphics/animation.hpp"
#include "graphics/animationsystem.hpp"
#include "graphics/sprite.hpp"

namespace
{
	using engine::graphics::Animation;
	using engine::graphics::Clip;

	constexpr Clip kWalk{
	    .firstFrame = {.x = 10, .y = 20, .width = 8, .height = 8}, .frames = 3, .secondsPerFrame = 0.25f};
} // namespace

static_assert(engine::ecs::Component<Animation>);

TEST_CASE("A looping clip steps through its frames and wraps round", "[graphics][animation]")
{
	Animation animation{.clip = kWalk};
	CHECK(engine::graphics::AnimationFrame(animation) == 0);

	animation = engine::graphics::Advance(animation, 0.2f);
	CHECK(engine::graphics::AnimationFrame(animation) == 0);
	animation = engine::graphics::Advance(animation, 0.1f);
	CHECK(engine::graphics::AnimationFrame(animation) == 1);
	animation = engine::graphics::Advance(animation, 0.5f);
	CHECK(engine::graphics::AnimationFrame(animation) == 0); // 0.8 s: round again

	// Many loops later it is still on the right frame.
	animation = engine::graphics::Advance(Animation{.clip = kWalk}, 1000.0f * 0.75f + 0.6f);
	CHECK(engine::graphics::AnimationFrame(animation) == 2);
}

TEST_CASE("A clip that doesn't loop stops on its last frame", "[graphics][animation]")
{
	Clip once = kWalk;
	once.loop = false;
	const Animation animation = engine::graphics::Advance(Animation{.clip = once}, 10.0f);
	CHECK(engine::graphics::AnimationFrame(animation) == 2);
}

TEST_CASE("The frame shown is that many frame-widths along the strip", "[graphics][animation]")
{
	const Animation animation = engine::graphics::Advance(Animation{.clip = kWalk}, 0.5f);
	const auto source = engine::graphics::AnimationSource(animation);
	CHECK(source.x == 26);
	CHECK(source.y == 20);
	CHECK(source.width == 8);
	CHECK(source.height == 8);
}

TEST_CASE("Playing the same clip carries on, and a new clip starts from its first frame", "[graphics][animation]")
{
	Animation animation = engine::graphics::Advance(Animation{.clip = kWalk}, 0.3f);
	animation = engine::graphics::Play(animation, kWalk);
	CHECK(engine::graphics::AnimationFrame(animation) == 1);

	Clip other = kWalk;
	other.firstFrame.y = 40;
	animation = engine::graphics::Play(animation, other);
	CHECK(engine::graphics::AnimationFrame(animation) == 0);
	CHECK(animation.elapsed == 0.0f);
}

TEST_CASE("Single frames and bad timings show the first frame, and time never runs backwards", "[graphics][animation]")
{
	const Animation still = engine::graphics::Advance(Animation{.clip = Clip{.frames = 1}}, 5.0f);
	CHECK(engine::graphics::AnimationFrame(still) == 0);
	const Animation zero =
	    engine::graphics::Advance(Animation{.clip = Clip{.frames = 4, .secondsPerFrame = 0.0f}}, 5.0f);
	CHECK(engine::graphics::AnimationFrame(zero) == 0);
	const Animation back = engine::graphics::Advance(Animation{.clip = kWalk}, -3.0f);
	CHECK(back.elapsed == 0.0f);
}

TEST_CASE("AnimationSystem steps each animation and shows its frame in the sprite", "[graphics][animation]")
{
	engine::ecs::World world;
	const engine::ecs::Entity walker = world.registry.Create();
	world.registry.Add<Animation>(walker, Animation{.clip = kWalk});
	world.registry.Add<engine::graphics::Sprite>(walker);
	const engine::ecs::Entity still = world.registry.Create(); // no Sprite: left alone
	world.registry.Add<Animation>(still, Animation{.clip = kWalk});

	engine::graphics::AnimationSystem(world, engine::ecs::FrameContext{.dt = 0.3});

	CHECK(engine::graphics::AnimationFrame(world.registry.GetComponent<Animation>(walker)) == 1);
	CHECK(world.registry.GetComponent<engine::graphics::Sprite>(walker).source.x == 18);
	CHECK(world.registry.GetComponent<Animation>(still).elapsed == 0.0f);
}
