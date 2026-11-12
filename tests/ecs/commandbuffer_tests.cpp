#include <catch2/catch_test_macros.hpp>

#include "ecs/commandbuffer.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include <memory>
#include <vector>

namespace
{
	struct Position
	{
		float x = 0;
		float y = 0;
	};

	struct Tag
	{
		int value = 0;
	};

	struct Owned // move-only component
	{
		std::unique_ptr<int> value;
	};

	using engine::ecs::CommandBuffer;
	using engine::ecs::Entity;
	using engine::ecs::Registry;
} // namespace

TEST_CASE("CommandBuffer::Destroy during Each leaves the right survivors", "[ecs][commandbuffer]")
{
	Registry registry;
	std::vector<Entity> entities;
	for (int i = 0; i < 10; ++i)
	{
		const Entity e = registry.Create();
		registry.Add<Tag>(e, i);
		entities.push_back(e);
	}

	CommandBuffer commands;
	int visited = 0;
	registry.Each<Tag>([&](Entity e, Tag& tag) {
		++visited;
		if (tag.value % 2 == 0) commands.Destroy(e);
	});

	// Nothing changes until Apply, and every entity was visited exactly once.
	CHECK(visited == 10);
	CHECK(commands.Size() == 5);
	for (const Entity e : entities)
	{
		CHECK(registry.IsAlive(e));
	}

	commands.Apply(registry);

	for (std::size_t i = 0; i < entities.size(); ++i)
	{
		const bool shouldSurvive = i % 2 == 1;
		CHECK(registry.IsAlive(entities[i]) == shouldSurvive);
		CHECK(registry.HasComponent<Tag>(entities[i]) == shouldSurvive);
	}

	int remaining = 0;
	registry.Each<Tag>([&](Entity, Tag& tag) {
		++remaining;
		CHECK(tag.value % 2 == 1);
	});
	CHECK(remaining == 5);
}

TEST_CASE("CommandBuffer::Add during Each is applied afterwards", "[ecs][commandbuffer]")
{
	Registry registry;
	const Entity a = registry.Create();
	const Entity b = registry.Create();
	registry.Add<Tag>(a, 1);
	registry.Add<Tag>(b, 2);

	CommandBuffer commands;
	registry.Each<Tag>([&](Entity e, Tag& tag) {
		commands.Add<Position>(e, static_cast<float>(tag.value), 0.0f);
		commands.Add<Tag>(registry.Create(), tag.value * 10); // new entities are fine to create directly
	});

	CHECK_FALSE(registry.HasComponent<Position>(a));

	commands.Apply(registry);

	REQUIRE(registry.HasComponent<Position>(a));
	REQUIRE(registry.HasComponent<Position>(b));
	CHECK(registry.GetComponent<Position>(a).x == 1.0f);
	CHECK(registry.GetComponent<Position>(b).x == 2.0f);

	int tagged = 0;
	registry.Each<Tag>([&](Entity, Tag&) { ++tagged; });
	CHECK(tagged == 4);
}

TEST_CASE("CommandBuffer::Remove removes only the recorded component", "[ecs][commandbuffer]")
{
	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Tag>(e, 1);
	registry.Add<Position>(e, 1.0f, 1.0f);

	CommandBuffer commands;
	commands.Remove<Position>(e);
	commands.Apply(registry);

	CHECK(registry.IsAlive(e));
	CHECK(registry.HasComponent<Tag>(e));
	CHECK_FALSE(registry.HasComponent<Position>(e));
}

TEST_CASE("CommandBuffer runs commands in order and skips dead entities", "[ecs][commandbuffer]")
{
	Registry registry;
	const Entity e = registry.Create();

	CommandBuffer commands;
	commands.Destroy(e);
	commands.Add<Tag>(e, 1);   // e is dead by the time this runs: skipped
	commands.Destroy(e);       // destroying twice is harmless

	commands.Apply(registry);

	CHECK_FALSE(registry.IsAlive(e));

	// The recycled id must not have picked up the Tag meant for the old entity.
	const Entity reused = registry.Create();
	CHECK(reused.id == e.id);
	CHECK_FALSE(registry.HasComponent<Tag>(reused));
}

TEST_CASE("CommandBuffer::Apply empties the buffer", "[ecs][commandbuffer]")
{
	Registry registry;
	const Entity e = registry.Create();

	CommandBuffer commands;
	REQUIRE(commands.Empty());

	commands.Add<Tag>(e, 7);
	CHECK(commands.Size() == 1);

	commands.Apply(registry);
	CHECK(commands.Empty());

	// Applying again does nothing (and doesn't trip the duplicate-component assert).
	commands.Apply(registry);
	CHECK(registry.GetComponent<Tag>(e).value == 7);

	SECTION("Clear drops commands without applying them")
	{
		commands.Remove<Tag>(e);
		commands.Clear();
		commands.Apply(registry);
		CHECK(registry.HasComponent<Tag>(e));
	}
}

TEST_CASE("CommandBuffer supports move-only components", "[ecs][commandbuffer]")
{
	Registry registry;
	const Entity e = registry.Create();

	CommandBuffer commands;
	commands.Add<Owned>(e, std::make_unique<int>(42));
	commands.Apply(registry);

	REQUIRE(registry.HasComponent<Owned>(e));
	REQUIRE(registry.GetComponent<Owned>(e).value != nullptr);
	CHECK(*registry.GetComponent<Owned>(e).value == 42);
}
