#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// The decisions a mixer makes, as pure functions: which voice a new sound takes, what a bus's volume does to a
// voice's gain, how far a pitch varies, and the gains of a crossfade. No SDL, so the tests can check them.
namespace engine::audio
{
	// Sounds are mixed in groups, each with its own volume (a music, effects and UI volume slider).
	enum class Bus : std::uint8_t
	{
		Music,
		Sfx,
		Ui,
		Count
	};
	inline constexpr std::size_t kBusCount = static_cast<std::size_t>(Bus::Count);

	struct PlayOptions
	{
		Bus bus = Bus::Sfx;
		float volume = 1.0f;        // 0 to 1, before the bus volume
		float pitchVariance = 0.0f; // 0 for none; 0.1 plays at a random speed between 0.9 and 1.1 of normal
		int priority = 0;           // a sound with a higher priority may cut off one with a lower
	};

	// What the mixer knows of each voice when it has to pick one for a new sound.
	struct VoiceState
	{
		bool usable = true; // false for a voice whose stream could not be made: never chosen
		bool busy = false;
		int priority = 0;
		std::uint64_t serial = 0; // counts up with each sound started, so a smaller one is older
	};

	// The voice a new sound with this priority should use: a free one, else the oldest of the voices with the lowest
	// priority, provided that priority is no higher than the new sound's. Returns voices.size() when every voice is
	// busy with something more important (or unusable), and the new sound is dropped.
	[[nodiscard]] std::size_t ChooseVoice(std::span<const VoiceState> voices, int priority) noexcept;

	// A volume is kept between 0 and 1; anything else (including NaN) becomes 0 or 1 rather than going on to SDL.
	[[nodiscard]] float ClampVolume(float volume) noexcept;

	// The gain of one voice: the sound's own volume times its bus's volume times the master volume.
	[[nodiscard]] float VoiceGain(float volume, float busVolume, float masterVolume) noexcept;

	// The playback speed for a sound with this pitchVariance. random is in [0, 1]; 0.5 gives exactly 1. The ratio stays
	// within SDL's limits.
	[[nodiscard]] float PitchRatio(float pitchVariance, float random) noexcept;

	// Equal-power crossfade at progress t (0 to 1, clamped): the gain of the track going out and of the one coming in.
	// out^2 + in^2 is always 1, so the loudness stays level, and both are smooth at the ends.
	struct CrossfadeGains
	{
		float out = 1.0f;
		float in = 0.0f;
	};
	[[nodiscard]] CrossfadeGains Crossfade(float t) noexcept;

	// Where a fade is after `elapsed` of `total` frames, on the way from `from` to `to`, as a straight line. A total of
	// 0 is already at `to`.
	[[nodiscard]] float FadeGain(float from, float to, std::uint64_t elapsed, std::uint64_t total) noexcept;

	// Like FadeGain, but along the equal-power curves of Crossfade: a fade up follows `in` and a fade down follows
	// `out`, so one track going down while another goes up over the same time keeps the loudness level. The ends are
	// exactly `from` and `to`.
	[[nodiscard]] float EqualPowerFade(float from, float to, std::uint64_t elapsed, std::uint64_t total) noexcept;
} // namespace engine::audio
