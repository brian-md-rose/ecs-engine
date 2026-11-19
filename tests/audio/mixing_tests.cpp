#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <limits>

#include "audio/mixing.hpp"

namespace
{
	using Catch::Matchers::WithinAbs;
	using engine::audio::ChooseVoice;
	using engine::audio::VoiceState;
} // namespace

TEST_CASE("A free voice is taken first", "[audio][mixing]")
{
	const std::array<VoiceState, 3> voices{{{.busy = true, .priority = 0, .serial = 1},
	                                        {.busy = false, .priority = 0, .serial = 0},
	                                        {.busy = true, .priority = 0, .serial = 2}}};
	CHECK(ChooseVoice(voices, 0) == 1);
	CHECK(ChooseVoice(voices, -5) == 1); // even a lowest-priority sound
}

TEST_CASE("With every voice busy the oldest of the lowest priority is replaced", "[audio][mixing]")
{
	const std::array<VoiceState, 4> voices{{{.busy = true, .priority = 5, .serial = 1},
	                                        {.busy = true, .priority = 1, .serial = 7},
	                                        {.busy = true, .priority = 1, .serial = 3},
	                                        {.busy = true, .priority = 2, .serial = 2}}};
	CHECK(ChooseVoice(voices, 1) == 2); // priority 1, and 3 is older than 7
	CHECK(ChooseVoice(voices, 9) == 2);
}

TEST_CASE("A sound that is less important than everything playing is dropped", "[audio][mixing]")
{
	const std::array<VoiceState, 2> voices{
	    {{.busy = true, .priority = 3, .serial = 1}, {.busy = true, .priority = 4, .serial = 2}}};
	CHECK(ChooseVoice(voices, 2) == voices.size());
	CHECK(ChooseVoice(voices, 3) == 0); // equal priority may replace the older
	CHECK(ChooseVoice({}, 0) == 0);     // no voices at all: nothing to choose, so "none"
}

TEST_CASE("Volumes are clamped and multiplied", "[audio][mixing]")
{
	using namespace engine::audio;
	CHECK(ClampVolume(-1.0f) == 0.0f);
	CHECK(ClampVolume(2.0f) == 1.0f);
	CHECK(ClampVolume(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
	CHECK(ClampVolume(0.25f) == 0.25f);
	CHECK_THAT(VoiceGain(0.5f, 0.5f, 1.0f), WithinAbs(0.25, 1e-6));
	CHECK_THAT(VoiceGain(0.5f, 0.5f, 0.5f), WithinAbs(0.125, 1e-6));
	CHECK(VoiceGain(1.0f, 0.0f, 1.0f) == 0.0f); // a muted bus silences the voice
	CHECK(VoiceGain(5.0f, 5.0f, 5.0f) == 1.0f); // never louder than the file
}

TEST_CASE("Pitch varies around 1 within the variance", "[audio][mixing]")
{
	using namespace engine::audio;
	CHECK(PitchRatio(0.0f, 0.0f) == 1.0f);
	CHECK(PitchRatio(0.0f, 1.0f) == 1.0f);
	CHECK_THAT(PitchRatio(0.1f, 0.5f), WithinAbs(1.0, 1e-6));
	CHECK_THAT(PitchRatio(0.1f, 0.0f), WithinAbs(0.9, 1e-6));
	CHECK_THAT(PitchRatio(0.1f, 1.0f), WithinAbs(1.1, 1e-6));
	// Silly inputs stay in a sane range.
	CHECK(PitchRatio(50.0f, 0.0f) > 0.0f);
	CHECK(PitchRatio(-1.0f, 0.3f) == 1.0f);
	CHECK(PitchRatio(std::numeric_limits<float>::quiet_NaN(), 0.3f) == 1.0f);
}

TEST_CASE("A crossfade keeps the loudness level and is smooth at the ends", "[audio][mixing]")
{
	using namespace engine::audio;
	CHECK_THAT(Crossfade(0.0f).out, WithinAbs(1.0, 1e-6));
	CHECK_THAT(Crossfade(0.0f).in, WithinAbs(0.0, 1e-6));
	CHECK_THAT(Crossfade(1.0f).out, WithinAbs(0.0, 1e-6));
	CHECK_THAT(Crossfade(1.0f).in, WithinAbs(1.0, 1e-6));
	for (int i = 0; i <= 20; ++i)
	{
		const CrossfadeGains g = Crossfade(static_cast<float>(i) / 20.0f);
		CHECK_THAT(g.out * g.out + g.in * g.in, WithinAbs(1.0, 1e-5));
	}
	// Out only falls and in only rises.
	float previousOut = 2.0f;
	float previousIn = -1.0f;
	for (int i = 0; i <= 20; ++i)
	{
		const CrossfadeGains g = Crossfade(static_cast<float>(i) / 20.0f);
		CHECK(g.out <= previousOut);
		CHECK(g.in >= previousIn);
		previousOut = g.out;
		previousIn = g.in;
	}
	CHECK(Crossfade(-3.0f).out == 1.0f);
	CHECK(Crossfade(9.0f).in == 1.0f);
}

TEST_CASE("A fade moves in a straight line and then stays", "[audio][mixing]")
{
	using namespace engine::audio;
	CHECK(FadeGain(1.0f, 0.0f, 0, 100) == 1.0f);
	CHECK_THAT(FadeGain(1.0f, 0.0f, 25, 100), WithinAbs(0.75, 1e-6));
	CHECK(FadeGain(1.0f, 0.0f, 100, 100) == 0.0f);
	CHECK(FadeGain(1.0f, 0.0f, 500, 100) == 0.0f);
	CHECK(FadeGain(0.2f, 0.8f, 7, 0) == 0.8f); // no time: already there
}

TEST_CASE("An equal-power fade starts and ends exactly and only moves one way", "[audio][mixing]")
{
	using namespace engine::audio;
	CHECK(EqualPowerFade(0.0f, 1.0f, 0, 100) == 0.0f);
	CHECK(EqualPowerFade(0.0f, 1.0f, 100, 100) == 1.0f);
	CHECK(EqualPowerFade(1.0f, 0.0f, 0, 100) == 1.0f);
	CHECK(EqualPowerFade(1.0f, 0.0f, 100, 100) == 0.0f);
	float up = -1.0f;
	float down = 2.0f;
	for (std::uint64_t i = 0; i <= 100; ++i)
	{
		const float rising = EqualPowerFade(0.0f, 1.0f, i, 100);
		const float falling = EqualPowerFade(1.0f, 0.0f, i, 100);
		CHECK(rising >= up);
		CHECK(falling <= down);
		// Over the same time the two stay level in power.
		CHECK_THAT(rising * rising + falling * falling, WithinAbs(1.0, 1e-4));
		up = rising;
		down = falling;
	}
	// From a part-way level.
	CHECK_THAT(EqualPowerFade(0.4f, 0.0f, 0, 10), WithinAbs(0.4, 1e-6));
	CHECK(EqualPowerFade(0.4f, 0.0f, 10, 10) == 0.0f);
}

TEST_CASE("A voice that could not be made is never chosen", "[audio][mixing]")
{
	const std::array<VoiceState, 3> voices{{{.usable = false, .busy = false, .priority = 0, .serial = 0},
	                                        {.busy = true, .priority = 4, .serial = 1},
	                                        {.usable = false, .busy = true, .priority = 0, .serial = 2}}};
	CHECK(ChooseVoice(voices, 9) == 1); // the one usable voice, though it is busy and the others look idle or old
	CHECK(ChooseVoice(voices, 2) == voices.size());

	const std::array<VoiceState, 2> none{{{.usable = false}, {.usable = false}}};
	CHECK(ChooseVoice(none, 9) == none.size());
}
