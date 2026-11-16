#include "audio/sound.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace engine::audio
{
	namespace
	{
		// A tiny linear congruential generator: deterministic everywhere, unlike std's
		// distributions, so noise sounds the same on every platform.
		class NoiseSource
		{
			std::uint32_t state = 0x2545F491u;

		public:
			[[nodiscard]] float Next() noexcept
			{
				state = state * 1664525u + 1013904223u;
				return static_cast<float>(state >> 8) / static_cast<float>(1u << 23) - 1.0f; // [-1, 1)
			}
		};

		// Fade in over the attack and out over the release; 1 in between.
		[[nodiscard]] float Envelope(const Tone& tone, float t) noexcept
		{
			float gain = 1.0f;
			if (tone.attackSeconds > 0.0f) gain = std::min(gain, t / tone.attackSeconds);
			if (tone.releaseSeconds > 0.0f) gain = std::min(gain, (tone.seconds - t) / tone.releaseSeconds);
			return std::clamp(gain, 0.0f, 1.0f);
		}
	} // namespace

	Sound Synthesize(std::span<const Tone> tones)
	{
		Sound sound;
		NoiseSource noise;
		constexpr auto kRate = static_cast<float>(kSampleRate);

		for (const Tone& tone : tones)
		{
			const auto count = static_cast<std::size_t>(std::max(tone.seconds, 0.0f) * kRate);
			float phase = 0.0f; // cycles completed, fractional part only
			float held = 0.0f;  // the noise value being held until the next change
			for (std::size_t i = 0; i < count; ++i)
			{
				const float t = static_cast<float>(i) / kRate;
				const float progress = tone.seconds > 0.0f ? t / tone.seconds : 0.0f;
				const float hz = tone.startHz + (tone.endHz - tone.startHz) * progress;

				const float before = phase;
				phase += std::max(hz, 0.0f) / kRate;
				phase -= std::floor(phase);

				float value = 0.0f;
				switch (tone.wave)
				{
				case Waveform::Square:
					value = phase < tone.duty ? 1.0f : -1.0f;
					break;
				case Waveform::Triangle:
					value = 4.0f * std::abs(phase - 0.5f) - 1.0f;
					break;
				case Waveform::Noise:
					if (i == 0 || phase < before) held = noise.Next(); // a new value each cycle
					value = held;
					break;
				case Waveform::Silence:
					break;
				}
				sound.samples.push_back(value * tone.volume * Envelope(tone, t));
			}
		}
		return sound;
	}
} // namespace engine::audio
