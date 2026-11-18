#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

#include "core/random.hpp"

namespace
{
	using engine::core::Random;
	using engine::core::RandomState;
} // namespace

// These golden values must come out the same on MSVC, GCC and Clang, which is the whole point of
// not using <random>. The first test is PCG's own published demo output, so it also checks that
// the generator really is PCG32.
TEST_CASE("Random matches the reference PCG32 output", "[core][random]")
{
	Random random(42, 54);
	const std::array<std::uint32_t, 6> expected{
	    {0xa15c02b7U, 0x7b47f409U, 0xba1d3330U, 0x83d2f293U, 0xbfa4784bU, 0xcbed606eU}};
	for (const std::uint32_t value : expected)
		CHECK(random.Next() == value);
}

TEST_CASE("Random golden values for seed 42", "[core][random]")
{
	Random next(42);
	const std::array<std::uint32_t, 8> expectedNext{
	    {0x21b756eeU, 0xc15ef750U, 0x9548a9bdU, 0x35db428dU, 0xf0071649U, 0xa243807fU, 0xb4c5bdd2U, 0x103ca9d2U}};
	for (const std::uint32_t value : expectedNext)
		CHECK(next.Next() == value);

	Random range(42);
	const std::array<int, 8> expectedRange{{1, 5, 4, 2, 6, 4, 5, 1}};
	for (const int value : expectedRange)
		CHECK(range.Range(1, 6) == value);

	// Unit is whole multiples of 2^-24, so these float values are exact, not approximate.
	Random unit(42);
	const std::array<std::uint32_t, 4> top24{{0x21b756U, 0xc15ef7U, 0x9548a9U, 0x35db42U}};
	for (const std::uint32_t bits : top24)
		CHECK(unit.Unit() == static_cast<float>(bits) / 16777216.0F);
}

TEST_CASE("The same seed gives the same sequence, and different seeds differ", "[core][random]")
{
	Random a(7);
	Random b(7);
	Random c(8);
	bool differs = false;
	for (int i = 0; i < 100; ++i)
	{
		const std::uint32_t x = a.Next();
		CHECK(b.Next() == x);
		if (c.Next() != x) differs = true;
	}
	CHECK(differs);
}

TEST_CASE("Range stays inside its bounds and reaches both ends", "[core][random]")
{
	Random random(1);
	bool sawMin = false;
	bool sawMax = false;
	for (int i = 0; i < 2000; ++i)
	{
		const int value = random.Range(-3, 4);
		REQUIRE(value >= -3);
		REQUIRE(value <= 4);
		sawMin = sawMin || value == -3;
		sawMax = sawMax || value == 4;
	}
	CHECK(sawMin);
	CHECK(sawMax);

	CHECK(random.Range(5, 5) == 5);
	CHECK(random.Range(0, 1) <= 1);

	// The extremes of int must not overflow.
	constexpr int kMin = std::numeric_limits<int>::min();
	constexpr int kMax = std::numeric_limits<int>::max();
	for (int i = 0; i < 100; ++i)
	{
		const int value = random.Range(kMin, kMax);
		CHECK(value >= kMin);
		CHECK(value <= kMax);
	}
	CHECK(random.Range(kMax - 1, kMax) >= kMax - 1);
	CHECK(random.Range(kMin, kMin + 1) <= kMin + 1);
}

TEST_CASE("Range is roughly uniform", "[core][random]")
{
	// A fixed seed makes this deterministic. Each of 6 faces expects 10000 of 60000 rolls; with a
	// standard deviation near 91, a spread of 500 is more than five of them.
	Random random(2024);
	std::array<int, 6> counts{};
	for (int i = 0; i < 60000; ++i)
		++counts[static_cast<std::size_t>(random.Range(1, 6) - 1)];
	for (const int count : counts)
	{
		CHECK(count > 9500);
		CHECK(count < 10500);
	}
}

TEST_CASE("Unit is in [0, 1) and Chance follows its probability", "[core][random]")
{
	Random random(3);
	float sum = 0;
	for (int i = 0; i < 10000; ++i)
	{
		const float value = random.Unit();
		REQUIRE(value >= 0.0F);
		REQUIRE(value < 1.0F);
		sum += value;
	}
	CHECK(sum / 10000.0F > 0.48F);
	CHECK(sum / 10000.0F < 0.52F);

	for (int i = 0; i < 100; ++i)
	{
		CHECK_FALSE(random.Chance(0.0F));
		CHECK_FALSE(random.Chance(-1.0F));
		CHECK(random.Chance(1.0F));
		CHECK(random.Chance(2.0F));
	}
	int hits = 0;
	for (int i = 0; i < 10000; ++i)
		hits += random.Chance(0.25F) ? 1 : 0;
	CHECK(hits > 2300);
	CHECK(hits < 2700);
}

TEST_CASE("Shuffle makes a permutation, and the same one for the same seed", "[core][random]")
{
	std::vector<int> original(20);
	std::iota(original.begin(), original.end(), 0);

	std::vector<int> first = original;
	Random(5).Shuffle(std::span<int>(first));
	std::vector<int> second = original;
	Random(5).Shuffle(std::span<int>(second));
	CHECK(first == second);
	CHECK(first != original);
	CHECK(std::is_permutation(first.begin(), first.end(), original.begin()));

	// Every position gets every value over many shuffles (a loose check that it isn't biased).
	std::array<int, 3> firstSlot{};
	Random random(6);
	for (int i = 0; i < 3000; ++i)
	{
		std::array<int, 3> items{{0, 1, 2}};
		random.Shuffle(std::span<int>(items));
		++firstSlot[static_cast<std::size_t>(items[0])];
	}
	for (const int count : firstSlot)
	{
		CHECK(count > 850);
		CHECK(count < 1150);
	}
}

TEST_CASE("Shuffle handles no items and one item", "[core][random]")
{
	Random random(9);
	std::vector<int> none;
	random.Shuffle(std::span<int>(none));
	CHECK(none.empty());

	std::vector<int> one{42};
	const RandomState before = random.State();
	random.Shuffle(std::span<int>(one));
	CHECK(one == std::vector<int>{42});
	CHECK(random.State().state == before.state); // nothing to decide, so nothing drawn
}

TEST_CASE("PickWeighted follows the weights", "[core][random]")
{
	Random random(11);
	const std::array<float, 3> weights{{1.0F, 2.0F, 1.0F}};
	std::array<int, 3> counts{};
	for (int i = 0; i < 20000; ++i)
		++counts[random.PickWeighted(weights)];
	CHECK(counts[1] > 9500);
	CHECK(counts[1] < 10500);
	CHECK(counts[0] > 4500);
	CHECK(counts[2] > 4500);
}

TEST_CASE("PickWeighted edge cases", "[core][random]")
{
	Random random(12);

	// One item is always chosen.
	const std::array<float, 1> one{{3.0F}};
	for (int i = 0; i < 20; ++i)
		CHECK(random.PickWeighted(one) == 0);

	// Weights of zero or less are never chosen while some weight is above zero.
	const std::array<float, 4> mixed{{0.0F, -5.0F, 2.0F, 0.0F}};
	for (int i = 0; i < 200; ++i)
		CHECK(random.PickWeighted(mixed) == 2);

	// With no positive weight, every index is possible.
	const std::array<float, 3> zeros{{0.0F, 0.0F, 0.0F}};
	std::array<int, 3> counts{};
	for (int i = 0; i < 3000; ++i)
		++counts[random.PickWeighted(zeros)];
	for (const int count : counts)
		CHECK(count > 800);
}

TEST_CASE("Fork gives independent, repeatable streams", "[core][random]")
{
	const Random root(42);

	// The golden value pins Fork's mixing across compilers.
	Random offers = root.Fork("offers");
	CHECK(offers.Next() == 0xeaf0c583U);
	CHECK(offers.Next() == 0x733963b2U);
	CHECK(offers.Next() == 0x1537a932U);

	// The same name gives the same stream, however much the parent has been used.
	Random used(42);
	for (int i = 0; i < 1000; ++i)
		used.Next();
	Random again = used.Fork("offers");
	Random fresh = root.Fork("offers");
	for (int i = 0; i < 20; ++i)
		CHECK(again.Next() == fresh.Next());

	// Different names differ from each other and from the parent, and drawing from one stream
	// leaves the others alone.
	Random shop = root.Fork("shop");
	Random map = root.Fork("map");
	Random parent(42);
	int sameShopMap = 0;
	int sameShopParent = 0;
	for (int i = 0; i < 100; ++i)
	{
		const std::uint32_t s = shop.Next();
		sameShopMap += s == map.Next() ? 1 : 0;
		sameShopParent += s == parent.Next() ? 1 : 0;
	}
	CHECK(sameShopMap == 0);
	CHECK(sameShopParent == 0);

	Random mapAlone = root.Fork("map");
	Random mapBeside = root.Fork("map");
	Random noise = root.Fork("noise");
	for (int i = 0; i < 50; ++i)
	{
		noise.Next();
		CHECK(mapAlone.Next() == mapBeside.Next());
	}

	// Different seeds fork to different streams of the same name.
	CHECK(Random(1).Fork("map").Next() != Random(2).Fork("map").Next());
}

TEST_CASE("A saved state carries on exactly where it stopped", "[core][random]")
{
	Random random(77);
	for (int i = 0; i < 10; ++i)
		random.Next();

	const RandomState saved = random.State();
	Random restored(saved);
	for (int i = 0; i < 50; ++i)
		CHECK(restored.Next() == random.Next());

	// Forks after a restore match forks before it.
	Random original(77);
	for (int i = 0; i < 10; ++i)
		original.Next();
	Random restoredAgain(original.State());
	CHECK(restoredAgain.Fork("x").Next() == original.Fork("x").Next());
}
