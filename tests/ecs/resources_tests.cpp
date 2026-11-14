#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "assert_guard.hpp"
#include "core/assert.hpp"
#include "ecs/resources.hpp"
#include "ecs/world.hpp"

namespace
{
	using engine::ecs::Resources;
	using engine::ecs::World;
	using engine::test::AssertFailure;
	using engine::test::AssertsThrow;

	struct Score
	{
		int value = 0;
	};

	struct HighScore
	{
		int value = 0;
	};

	struct Named
	{
		std::string name;
		int level = 0;

		Named(std::string n, int l) : name(std::move(n)), level(l) {}
	};

	// Neither copyable nor movable, like Renderer2D.
	struct Pinned
	{
		int value;

		explicit Pinned(int v) : value(v) {}
		Pinned(const Pinned&) = delete;
		Pinned& operator=(const Pinned&) = delete;
		Pinned(Pinned&&) = delete;
		Pinned& operator=(Pinned&&) = delete;
	};

	// Records its id in a shared log when destroyed.
	struct Logged
	{
		std::vector<int>* log;
		int id;

		Logged(std::vector<int>* l, int i) : log(l), id(i) {}
		Logged(const Logged&) = delete;
		Logged& operator=(const Logged&) = delete;
		Logged(Logged&&) = delete;
		Logged& operator=(Logged&&) = delete;
		~Logged() { log->push_back(id); }
	};

	// Distinct trivial types, to fill a Resources up.
	template <int N> struct Filler
	{
		int value = N;
	};

	template <int N> struct Tag : Logged
	{
		Tag(std::vector<int>* l, int i) : Logged(l, i) {}
	};
} // namespace

TEST_CASE("Emplace constructs a resource that Get returns", "[ecs][resources]")
{
	Resources resources;
	Named& created = resources.Emplace<Named>("maze-1", 3);

	CHECK(created.name == "maze-1");
	CHECK(&resources.Get<Named>() == &created);
	CHECK(resources.Get<Named>().level == 3);

	resources.Get<Named>().level = 4;
	const Resources& readOnly = resources;
	CHECK(readOnly.Get<Named>().level == 4);
}

TEST_CASE("Each type holds its own value", "[ecs][resources]")
{
	Resources resources;
	resources.Emplace<Score>(10);
	resources.Emplace<HighScore>(500);

	CHECK(resources.Get<Score>().value == 10);
	CHECK(resources.Get<HighScore>().value == 500);
	CHECK(resources.Count() == 2);
}

TEST_CASE("TryGet and Has report missing resources without failing", "[ecs][resources]")
{
	Resources resources;
	CHECK_FALSE(resources.Has<Score>());
	CHECK(resources.TryGet<Score>() == nullptr);

	resources.Emplace<Score>();
	CHECK(resources.Has<Score>());
	REQUIRE(resources.TryGet<Score>() != nullptr);
	CHECK(resources.TryGet<Score>()->value == 0);
}

TEST_CASE("Get on a missing resource fails in every configuration", "[ecs][resources]")
{
	// ENGINE_VERIFY, so this runs in Release too.
	const AssertsThrow guard;
	Resources resources;
	const auto get = [&resources]
	{
		[[maybe_unused]] Score& score = resources.Get<Score>();
	};
	CHECK_THROWS_AS(get(), AssertFailure);
}

TEST_CASE("Emplacing a type twice asserts", "[ecs][resources][assert]")
{
	if constexpr (!engine::kAssertsEnabled)
	{
		SKIP("asserts are compiled out in this configuration");
	}

	const AssertsThrow guard;
	Resources resources;
	resources.Emplace<Score>(1);
	const auto again = [&resources]
	{
		resources.Emplace<Score>(2);
	};
	CHECK_THROWS_AS(again(), AssertFailure);
	CHECK(resources.Get<Score>().value == 1);
}

TEST_CASE("References stay valid as more resources are added", "[ecs][resources]")
{
	Resources resources;
	Score& score = resources.Emplace<Score>(7);

	// Enough insertions to make the map rehash.
	resources.Emplace<HighScore>();
	resources.Emplace<Named>("x", 0);
	resources.Emplace<Pinned>(1);
	resources.Emplace<Filler<1>>();
	resources.Emplace<Filler<2>>();
	resources.Emplace<Filler<3>>();
	resources.Emplace<Filler<4>>();
	resources.Emplace<Filler<5>>();
	resources.Emplace<Filler<6>>();
	resources.Emplace<Filler<7>>();
	resources.Emplace<Filler<8>>();

	CHECK(&resources.Get<Score>() == &score);
	CHECK(score.value == 7);
}

TEST_CASE("Resources needn't be copyable or movable", "[ecs][resources]")
{
	Resources resources;
	resources.Emplace<Pinned>(42);
	CHECK(resources.Get<Pinned>().value == 42);
}

TEST_CASE("Remove destroys a resource and allows adding it again", "[ecs][resources]")
{
	std::vector<int> log;
	Resources resources;
	resources.Emplace<Tag<1>>(&log, 1);
	resources.Emplace<Score>(5);

	resources.Remove<Tag<1>>();
	CHECK(log == std::vector<int>{1});
	CHECK_FALSE(resources.Has<Tag<1>>());
	CHECK(resources.Count() == 1);

	resources.Remove<Tag<1>>(); // absent: no-op
	CHECK(log.size() == 1);

	resources.Emplace<Tag<1>>(&log, 11);
	CHECK(resources.Get<Tag<1>>().id == 11);
	resources.Remove<Tag<1>>();
}

TEST_CASE("Resources are destroyed newest first", "[ecs][resources]")
{
	std::vector<int> log;
	{
		Resources resources;
		resources.Emplace<Tag<1>>(&log, 1);
		resources.Emplace<Tag<2>>(&log, 2);
		resources.Emplace<Tag<3>>(&log, 3);
		resources.Remove<Tag<2>>();
		resources.Emplace<Tag<2>>(&log, 4); // re-added, so now the newest
	}
	CHECK(log == std::vector<int>{2, 4, 3, 1});
}

TEST_CASE("A World bundles the registry, resources and commands", "[ecs][resources]")
{
	World world;
	const auto entity = world.registry.Create();
	world.resources.Emplace<Score>(3);
	world.commands.Add<Score>(entity, 9);
	world.commands.Apply(world.registry);

	CHECK(world.registry.GetComponent<Score>(entity).value == 9);
	CHECK(world.resources.Get<Score>().value == 3);
}
