#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include "core/assert.hpp"

// A seeded random number generator whose output is the same on every compiler and platform, so a
// seed rolled on Windows gives the same run on a Steam Deck. It doesn't use <random>: the standard
// distributions (uniform_int_distribution and friends) are allowed to differ between MSVC,
// libstdc++ and libc++. Pure: no SDL.
namespace engine::core
{
	// Everything needed to carry on a Random from where it stopped, for saving a run mid-way.
	struct RandomState
	{
		std::uint64_t state = 0;
		std::uint64_t increment = 0;
		std::uint64_t seed = 0; // kept so Fork gives the same streams after a restore
	};

	// PCG32 (the XSH RR variant). Cheap to copy, so a system can keep its own.
	class Random
	{
		std::uint64_t state = 0;
		std::uint64_t increment = 1; // always odd
		std::uint64_t seed = 0;

	public:
		// Different seeds give unrelated sequences. Different streams of the same seed give
		// sequences that are independent of each other, but see Fork for making those.
		explicit Random(std::uint64_t seed, std::uint64_t stream = 0);

		// Carries on from a saved state.
		explicit Random(const RandomState& saved);

		[[nodiscard]] RandomState State() const noexcept
		{
			return {.state = state, .increment = increment, .seed = seed};
		}

		// The next 32 random bits.
		std::uint32_t Next();

		// A whole number from min to maxInclusive, both included, with no modulo bias (Lemire's
		// method). min must not be above maxInclusive.
		int Range(int min, int maxInclusive);

		// A number from 0 up to but not including 1, from the top 24 bits so every value is
		// exactly representable.
		float Unit();

		// True with the given probability: always for 1 or more, never for 0 or less.
		bool Chance(float probability);

		// Puts items in a random order (Fisher-Yates). Every order is equally likely.
		template <class T> void Shuffle(std::span<T> items)
		{
			ENGINE_VERIFY(items.size() <= static_cast<std::size_t>(INT32_MAX), "too many items to shuffle");
			for (std::size_t i = items.size(); i > 1; --i)
			{
				const std::size_t j = static_cast<std::size_t>(Range(0, static_cast<int>(i) - 1));
				std::swap(items[i - 1], items[j]);
			}
		}

		// An index into weights, each chosen in proportion to its weight. A weight of 0 or less is
		// never chosen, unless none is above 0, and then every index is equally likely. weights
		// must not be empty.
		std::size_t PickWeighted(std::span<const float> weights);

		// A new generator for the stream called name, worked out from this one's seed and the name
		// alone. It doesn't depend on how many numbers have been drawn, and drawing from one stream
		// doesn't change another, so "offers", "shop" and "map" stay independent and keep their
		// sequences when a new stream is added.
		[[nodiscard]] Random Fork(std::string_view name) const;
	};
} // namespace engine::core
