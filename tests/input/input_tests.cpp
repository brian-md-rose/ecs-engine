#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>

#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "input/input.hpp"

namespace
{
	using engine::input::Input;
	using engine::input::Key;

	// Counts the lines of the key table, to check Key has an enumerator for each.
	constexpr std::size_t CountKeys()
	{
		std::size_t count = 0;
#define ENGINE_KEY_COUNTER(name, sdl) ++count;
		ENGINE_KEY_LIST(ENGINE_KEY_COUNTER)
#undef ENGINE_KEY_COUNTER
		return count;
	}

	// A resource a system writes, so a test can see what the system read from the input.
	struct Seen
	{
		bool jump = false;
		std::string typed;
	};

	void ReadInputSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context)
	{
		Seen& seen = world.resources.Get<Seen>();
		seen.jump = context.input->keyboard.WasPressed(Key::Space);
		seen.typed = context.input->typed;
	}
} // namespace

TEST_CASE("Key has one enumerator per line of the key table", "[input]")
{
	STATIC_REQUIRE(engine::input::kKeyCount == CountKeys());
	STATIC_REQUIRE(static_cast<int>(Key::Z) > static_cast<int>(Key::A));
}

TEST_CASE("The new keys report presses and releases like the old ones", "[input]")
{
	Input input;
	for (const Key key : {Key::Q, Key::Num0, Key::Num9, Key::F12, Key::Tab, Key::Backspace, Key::Delete, Key::Shift,
	                      Key::Ctrl, Key::Alt, Key::Home, Key::End, Key::PageUp, Key::PageDown})
	{
		input.keyboard.OnKeyDown(key);
		CHECK(input.keyboard.IsDown(key));
		CHECK(input.keyboard.WasPressed(key));
		input.EndUpdate();
		CHECK(input.keyboard.IsDown(key));
		CHECK_FALSE(input.keyboard.WasPressed(key));
		input.keyboard.OnKeyUp(key);
		CHECK(input.keyboard.WasReleased(key));
		CHECK_FALSE(input.keyboard.IsDown(key));
		input.EndUpdate();
	}
}

TEST_CASE("Typed text collects until the update that reads it has ended", "[input]")
{
	Input input;
	CHECK(input.typed.empty());

	input.OnText("Ad");
	input.OnText("a!");
	CHECK(input.typed == "Ada!");

	// Not cleared by a frame with no update, only by the end of one.
	input.OnText(" \xC3\xA9"); // an accented e, as UTF-8
	CHECK(input.typed == "Ada! \xC3\xA9");

	input.EndUpdate();
	CHECK(input.typed.empty());
}

TEST_CASE("ReleaseAll lets go of every key and drops pending text", "[input]")
{
	Input input;
	input.keyboard.OnKeyDown(Key::Shift);
	input.keyboard.OnKeyDown(Key::W);
	input.OnText("x");

	input.ReleaseAll();
	CHECK_FALSE(input.keyboard.IsDown(Key::Shift));
	CHECK_FALSE(input.keyboard.IsDown(Key::W));
	CHECK(input.keyboard.WasReleased(Key::W));
	CHECK(input.typed.empty());
}

TEST_CASE("A system reads a hand-built Input through the FrameContext", "[input]")
{
	engine::ecs::World world;
	world.resources.Emplace<Seen>();
	engine::ecs::Scheduler scheduler;
	scheduler.Add(engine::ecs::Phase::Input, "ReadInput", ReadInputSystem);

	Input input;
	input.keyboard.OnKeyDown(Key::Space);
	input.OnText("hi");
	scheduler.Run(engine::ecs::Phase::Input, world, engine::ecs::FrameContext{.input = &input});
	CHECK(world.resources.Get<Seen>().jump);
	CHECK(world.resources.Get<Seen>().typed == "hi");

	input.EndUpdate();
	scheduler.Run(engine::ecs::Phase::Input, world, engine::ecs::FrameContext{.input = &input});
	CHECK_FALSE(world.resources.Get<Seen>().jump);
	CHECK(world.resources.Get<Seen>().typed.empty());
}
