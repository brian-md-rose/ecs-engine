#include "core/random.hpp"

#include <limits>

namespace engine::core
{
	namespace
	{
		constexpr std::uint64_t kMultiplier = 6364136223846793005ULL;

		[[nodiscard]] std::uint32_t RotateRight(std::uint32_t value, std::uint32_t amount) noexcept
		{
			return (value >> amount) | (value << ((0U - amount) & 31U));
		}

		// SplitMix64: spreads a 64-bit input into well-mixed output, even for similar inputs.
		[[nodiscard]] std::uint64_t SplitMix64(std::uint64_t& x) noexcept
		{
			x += 0x9E3779B97F4A7C15ULL;
			std::uint64_t z = x;
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
			return z ^ (z >> 31);
		}

		[[nodiscard]] std::uint64_t HashName(std::string_view name) noexcept // FNV-1a, 64 bit
		{
			std::uint64_t hash = 0xCBF29CE484222325ULL;
			for (const char c : name)
			{
				hash ^= static_cast<std::uint8_t>(c);
				hash *= 0x100000001B3ULL;
			}
			return hash;
		}
	} // namespace

	Random::Random(std::uint64_t seedValue, std::uint64_t stream) : seed(seedValue)
	{
		// The reference seeding routine for PCG32.
		state = 0;
		increment = (stream << 1U) | 1U;
		Next();
		state += seedValue;
		Next();
	}

	Random::Random(const RandomState& saved) : state(saved.state), increment(saved.increment | 1U), seed(saved.seed)
	{
	}

	std::uint32_t Random::Next()
	{
		const std::uint64_t old = state;
		state = old * kMultiplier + increment;
		const auto xorShifted = static_cast<std::uint32_t>(((old >> 18U) ^ old) >> 27U);
		const auto rotation = static_cast<std::uint32_t>(old >> 59U);
		return RotateRight(xorShifted, rotation);
	}

	int Random::Range(int min, int maxInclusive)
	{
		ENGINE_VERIFY(min <= maxInclusive, "Random::Range needs min <= max");
		// The span is worked out in unsigned arithmetic so INT_MIN..INT_MAX doesn't overflow.
		const auto span =
		    static_cast<std::uint32_t>(static_cast<std::uint32_t>(maxInclusive) - static_cast<std::uint32_t>(min)) + 1U;
		if (span == 0) return static_cast<int>(Next()); // the whole range of int

		std::uint64_t product = static_cast<std::uint64_t>(Next()) * span;
		auto low = static_cast<std::uint32_t>(product);
		if (low < span)
		{
			const std::uint32_t threshold = (0U - span) % span; // values below this would be biased
			while (low < threshold)
			{
				product = static_cast<std::uint64_t>(Next()) * span;
				low = static_cast<std::uint32_t>(product);
			}
		}
		return static_cast<int>(static_cast<std::uint32_t>(min) + static_cast<std::uint32_t>(product >> 32U));
	}

	float Random::Unit()
	{
		return static_cast<float>(Next() >> 8U) * (1.0F / 16777216.0F);
	}

	bool Random::Chance(float probability)
	{
		return Unit() < probability;
	}

	std::size_t Random::PickWeighted(std::span<const float> weights)
	{
		ENGINE_VERIFY(!weights.empty(), "PickWeighted needs at least one weight");
		ENGINE_VERIFY(weights.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()), "too many weights");

		double total = 0;
		for (const float weight : weights)
		{
			if (weight > 0.0F) total += static_cast<double>(weight);
		}
		if (!(total > 0.0)) return static_cast<std::size_t>(Range(0, static_cast<int>(weights.size()) - 1));

		const double target = static_cast<double>(Unit()) * total;
		double sum = 0;
		std::size_t lastPositive = 0;
		for (std::size_t i = 0; i < weights.size(); ++i)
		{
			if (!(weights[i] > 0.0F)) continue;
			sum += static_cast<double>(weights[i]);
			lastPositive = i;
			if (target < sum) return i;
		}
		return lastPositive; // only reached through rounding at the very top
	}

	Random Random::Fork(std::string_view name) const
	{
		// Both the new state and the new increment come out of SplitMix64: two PCG32s that differ
		// only in the increment give correlated output.
		std::uint64_t mix = seed ^ HashName(name);
		const std::uint64_t newSeed = SplitMix64(mix);
		const std::uint64_t newStream = SplitMix64(mix);
		return Random(newSeed, newStream);
	}
} // namespace engine::core
