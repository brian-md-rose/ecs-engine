#include "audio/mixing.hpp"

#include <algorithm>
#include <cmath>

namespace engine::audio
{
	std::size_t ChooseVoice(std::span<const VoiceState> voices, int priority) noexcept
	{
		std::size_t best = voices.size();
		for (std::size_t i = 0; i < voices.size(); ++i)
		{
			if (!voices[i].usable) continue;
			if (!voices[i].busy) return i;
			const bool lower = best == voices.size() || voices[i].priority < voices[best].priority ||
			                   (voices[i].priority == voices[best].priority && voices[i].serial < voices[best].serial);
			if (lower) best = i;
		}
		if (best == voices.size() || voices[best].priority > priority) return voices.size();
		return best;
	}

	float ClampVolume(float volume) noexcept
	{
		if (!(volume > 0.0f)) return 0.0f; // also NaN
		return std::min(volume, 1.0f);
	}

	float VoiceGain(float volume, float busVolume, float masterVolume) noexcept
	{
		return ClampVolume(volume) * ClampVolume(busVolume) * ClampVolume(masterVolume);
	}

	float PitchRatio(float pitchVariance, float random) noexcept
	{
		const float variance = std::clamp(std::isfinite(pitchVariance) ? pitchVariance : 0.0f, 0.0f, 0.9f);
		const float r = std::clamp(std::isfinite(random) ? random : 0.5f, 0.0f, 1.0f);
		return 1.0f + variance * (2.0f * r - 1.0f);
	}

	CrossfadeGains Crossfade(float t) noexcept
	{
		constexpr float kHalfPi = 1.5707963267948966f;
		const float x = std::clamp(std::isfinite(t) ? t : 0.0f, 0.0f, 1.0f);
		return {.out = std::cos(x * kHalfPi), .in = std::sin(x * kHalfPi)};
	}

	float FadeGain(float from, float to, std::uint64_t elapsed, std::uint64_t total) noexcept
	{
		if (total == 0 || elapsed >= total) return to;
		const float t = static_cast<float>(elapsed) / static_cast<float>(total);
		return from + (to - from) * t;
	}

	float EqualPowerFade(float from, float to, std::uint64_t elapsed, std::uint64_t total) noexcept
	{
		if (total == 0 || elapsed >= total) return to;
		const CrossfadeGains g = Crossfade(static_cast<float>(elapsed) / static_cast<float>(total));
		return to >= from ? from + (to - from) * g.in : to + (from - to) * g.out;
	}
} // namespace engine::audio
