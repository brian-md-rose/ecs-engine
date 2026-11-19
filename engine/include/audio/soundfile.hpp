#pragma once
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

	// Reads a .wav (any format SDL reads, converted to float) or an .ogg (Ogg Vorbis). The Sound keeps the
	// file's own sample rate and channel count; nothing is resampled here.
	[[nodiscard]] SoundResult LoadSound(const std::filesystem::path& path);
} // namespace engine::audio
