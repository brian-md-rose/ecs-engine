#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "audio/sound.hpp"

namespace
{
	using engine::audio::kSampleRate;
	using engine::audio::Sound;
	using engine::audio::Tone;
	using engine::audio::Waveform;

	[[nodiscard]] float Peak(const Sound& sound)
	{
		float peak = 0.0f;
		for (const float sample : sound.samples)
		{
			peak = std::max(peak, std::abs(sample));
		}
		return peak;
	}
} // namespace

TEST_CASE("Tones play one after another for their full length", "[audio][synth]")
{
	const std::array tones = {Tone{.seconds = 0.1f}, Tone{.wave = Waveform::Silence, .seconds = 0.05f}};
	const Sound sound = engine::audio::Synthesize(tones);
	CHECK(sound.samples.size() == static_cast<std::size_t>(0.15f * kSampleRate));
	CHECK(sound.Seconds() > 0.149);
	CHECK(sound.Seconds() < 0.151);
	CHECK(sound.samples.back() == 0.0f); // the rest is silent
}

TEST_CASE("A tone never goes above its volume and fades in and out", "[audio][synth]")
{
	for (const Waveform wave : {Waveform::Square, Waveform::Triangle, Waveform::Noise})
	{
		const std::array tones = {Tone{.wave = wave,
		                               .startHz = 300.0f,
		                               .endHz = 900.0f,
		                               .seconds = 0.2f,
		                               .volume = 0.4f,
		                               .attackSeconds = 0.01f,
		                               .releaseSeconds = 0.05f}};
		const Sound sound = engine::audio::Synthesize(tones);
		REQUIRE_FALSE(sound.samples.empty());
		CHECK(Peak(sound) <= 0.4f + 1e-6f);
		CHECK(Peak(sound) > 0.2f);
		CHECK(std::abs(sound.samples.front()) < 0.01f); // no click at the start
		CHECK(std::abs(sound.samples.back()) < 0.01f);  // nor at the end
	}
}

TEST_CASE("A square wave's duty sets how long each cycle is high", "[audio][synth]")
{
	// 441 Hz at 44100 samples per second: exactly 100 samples a cycle.
	const std::array tones = {Tone{.startHz = 441.0f,
	                               .endHz = 441.0f,
	                               .seconds = 0.1f,
	                               .volume = 1.0f,
	                               .attackSeconds = 0.0f,
	                               .releaseSeconds = 0.0f,
	                               .duty = 0.25f}};
	const Sound sound = engine::audio::Synthesize(tones);
	const auto high = std::count_if(sound.samples.begin(), sound.samples.end(), [](float sample)
	{
		return sample > 0.0f;
	});
	CHECK(static_cast<double>(high) / static_cast<double>(sound.samples.size()) > 0.23);
	CHECK(static_cast<double>(high) / static_cast<double>(sound.samples.size()) < 0.27);
}

TEST_CASE("Noise is the same every time", "[audio][synth]")
{
	const std::array tones = {Tone{.wave = Waveform::Noise, .startHz = 4000.0f, .endHz = 4000.0f}};
	CHECK(engine::audio::Synthesize(tones).samples == engine::audio::Synthesize(tones).samples);
}
