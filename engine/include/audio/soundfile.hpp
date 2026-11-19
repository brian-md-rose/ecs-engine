#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

#include "audio/sound.hpp"

namespace engine::audio
{
	// What LoadSound gives back: the Sound, or why there isn't one. A bad file is an error to report, never
	// a crash.
	struct SoundResult
	{
		Sound sound;
		std::string error; // empty when the file loaded

		[[nodiscard]] bool Ok() const noexcept { return error.empty(); }
	};

	// Files bigger than this are refused, so a wrong path can't read gigabytes.
	inline constexpr std::uintmax_t kMaxSoundFileBytes = 256ull * 1024 * 1024;

	// A decoded sound (as float samples) bigger than this is refused: a small Ogg file can unpack to many times its
	// size.
	inline constexpr std::uintmax_t kMaxDecodedSoundBytes = kMaxSoundFileBytes;

	// Whether a sound of this many frames and channels, as floats, is within kMaxDecodedSoundBytes.
	[[nodiscard]] bool DecodedSizeAllowed(std::uint64_t frames, int channels) noexcept;

	// Reads a .wav (any format SDL reads, converted to float) or an .ogg (Ogg Vorbis). The Sound keeps the
	// file's own sample rate and channel count; nothing is resampled here.
	[[nodiscard]] SoundResult LoadSound(const std::filesystem::path& path);
} // namespace engine::audio
