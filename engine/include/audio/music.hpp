#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Only forward-declared: see graphics/renderer.hpp. Only engine .cpp files include <SDL3/...>.
struct SDL_AudioStream;
struct stb_vorbis;

namespace engine::audio
{
	// One piece of music streaming from an Ogg Vorbis file. SDL asks for more samples from its audio thread, and the
	// track decodes them there, a few thousand at a time, so a slow frame on the main thread can't starve it. It
	// loops without a gap (when the decoder reaches the end it starts again inside the same buffer), and it fades
	// in or out sample by sample, so two tracks fading in opposite directions make a crossfade with no click.
	//
	// The main thread makes it, sets its fades and volume, and destroys it. Everything in between is the audio
	// thread's.
	class MusicTrack
	{
		// What the audio thread works from. Set once, in Open.
		std::vector<unsigned char> file; // the whole compressed file, which the decoder reads from
		stb_vorbis* decoder = nullptr;
		SDL_AudioStream* stream = nullptr;
		int channels = 0;
		int sampleRate = 0;
		std::vector<float> chunk;

		// Fade requests from the main thread: a serial number says there is a new one.
		std::atomic<std::uint32_t> requestSerial{0};
		std::atomic<float> requestTarget{0.0f};
		std::atomic<std::uint64_t> requestFrames{0};

		// The audio thread's own fade state.
		std::uint32_t seenSerial = 0;
		float fadeFrom = 0.0f;
		float fadeTo = 0.0f;
		float gain = 0.0f;
		std::uint64_t fadeElapsed = 0;
		std::uint64_t fadeTotal = 0;

		std::atomic<bool> silent{false}; // faded all the way out, or the file ended with an error

		MusicTrack() = default;
		void Fill(int additionalBytes) noexcept;
		static void OnStreamNeedsData(void* userdata, SDL_AudioStream* audioStream, int additional, int total) noexcept;

	public:
		// Opens the file and starts a stream bound to `device` that plays, from silence, once SetFade raises the gain.
		// Returns null with `error` set if the file can't be read as Ogg Vorbis or the stream can't be made.
		[[nodiscard]] static std::unique_ptr<MusicTrack> Open(std::uint32_t device, const std::filesystem::path& path,
		                                                      std::string& error);
		~MusicTrack();
		MusicTrack(const MusicTrack&) = delete;
		MusicTrack& operator=(const MusicTrack&) = delete;
		MusicTrack(MusicTrack&&) = delete;
		MusicTrack& operator=(MusicTrack&&) = delete;

		// Moves the track's own level to `target` (0 to 1) over `seconds`, on an equal-power curve, from wherever it
		// is.
		void FadeTo(float target, double seconds);

		// The level of the music bus, applied to the whole stream and so separate from the fades.
		void SetVolume(float volume);

		// True once the track has faded to nothing; it can be destroyed without cutting anything off.
		[[nodiscard]] bool IsSilent() const noexcept { return silent.load(std::memory_order_relaxed); }

		[[nodiscard]] int SampleRate() const noexcept { return sampleRate; }
	};
} // namespace engine::audio
