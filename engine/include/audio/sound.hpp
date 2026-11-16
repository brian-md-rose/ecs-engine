#pragma once
#include <cstdint>
#include <span>
#include <vector>

// Sounds as plain sample data, and a small synthesiser that makes them from tones, so a game can
// have original sound effects without any sound files. Pure: no SDL, so the tests can check it.
namespace engine::audio
{
	// Every Sound is mono, 32-bit float in [-1, 1], at this many samples per second. The
	// AudioDevice plays it at this rate whatever the hardware's.
	inline constexpr int kSampleRate = 44100;

	struct Sound
	{
		std::vector<float> samples;

		[[nodiscard]] double Seconds() const noexcept
		{
			return static_cast<double>(samples.size()) / static_cast<double>(kSampleRate);
		}
	};

	enum class Waveform : std::uint8_t
	{
		Square,   // buzzy; duty sets how much of each cycle is high
		Triangle, // soft and round
		Noise,    // hiss and crunch; the frequency sets how often it changes value
		Silence   // a rest
	};

	// One note, with its pitch sliding from startHz to endHz in a straight line. It fades in over
	// attackSeconds and out over its last releaseSeconds, so it starts and ends without a click.
	struct Tone
	{
		Waveform wave = Waveform::Square;
		float startHz = 440.0f;
		float endHz = 440.0f;
		float seconds = 0.1f;
		float volume = 0.25f; // peak amplitude, 0-1
		float attackSeconds = 0.005f;
		float releaseSeconds = 0.02f;
		float duty = 0.5f; // Square only: the fraction of each cycle that is high
	};

	// The tones one after another. Noise comes from a fixed-seed generator, so the same tones
	// always give the same samples.
	[[nodiscard]] Sound Synthesize(std::span<const Tone> tones);
} // namespace engine::audio
