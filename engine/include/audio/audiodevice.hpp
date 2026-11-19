#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "audio/mixing.hpp"

// Only forward-declared: see graphics/renderer.hpp. Only engine .cpp files include <SDL3/...>.
struct SDL_AudioStream;

namespace engine::audio
{
	struct Sound;
	class MusicTrack;

	// Owns SDL's audio subsystem and the default playback device, and plays Sounds on it, several at once, in three
	// buses (music, effects, UI) with a volume each. If there is no device (a machine without sound, a CI runner), the
	// reason is logged once and everything does nothing, so a game runs the same with or without sound.
	class AudioDevice
	{
		static constexpr std::size_t kVoices = 32; // sounds that can play at the same time

		struct Voice
		{
			SDL_AudioStream* stream = nullptr;
			Bus bus = Bus::Sfx;
			int priority = 0;
			std::uint64_t serial = 0;
			float volume = 1.0f;
		};

		std::uint32_t device = 0; // SDL_AudioDeviceID; 0 when there is none
		std::array<Voice, kVoices> voices{};
		std::uint64_t nextSerial = 1;
		std::uint32_t randomState = 0x9e3779b9u; // for pitch variance
		std::array<float, kBusCount> busVolumes{{1.0f, 1.0f, 1.0f}};
		float masterVolume = 1.0f;
		bool subsystemStarted = false;

		std::unique_ptr<MusicTrack> music;                // what is playing now
		std::vector<std::unique_ptr<MusicTrack>> leaving; // fading out; removed once silent
		std::filesystem::path musicPath;

		[[nodiscard]] bool IsBusy(const Voice& voice) const;
		void ApplyGain(const Voice& voice) const;
		void ApplyMusicVolume() const;
		void ReapMusic();
		float NextRandom();

	public:
		// Opens the default playback device. Needs SDL started (Application's SdlScope).
		AudioDevice();
		~AudioDevice();

		AudioDevice(const AudioDevice&) = delete;
		AudioDevice& operator=(const AudioDevice&) = delete;
		AudioDevice(AudioDevice&&) = delete;
		AudioDevice& operator=(AudioDevice&&) = delete;

		// False if no device could be opened; everything is then silent.
		[[nodiscard]] bool IsAvailable() const noexcept { return device != 0; }

		// Starts sound now, alongside whatever is playing. With every voice busy, it replaces the oldest of the lowest
		// priority, or is dropped if all of them matter more (see ChooseVoice). The samples are copied, so sound
		// needn't outlive the call. The sound plays at its own sample rate and channel count; SDL converts.
		void Play(const Sound& sound, const PlayOptions& options = {});

		// The volume of a bus, 0 to 1, applied at once to what is playing on it. And a master volume over all of them.
		void SetBusVolume(Bus bus, float volume);
		[[nodiscard]] float BusVolume(Bus bus) const noexcept;
		void SetMasterVolume(float volume);
		[[nodiscard]] float MasterVolume() const noexcept { return masterVolume; }

		// Call once a frame: removes the tracks that have finished fading out, which would otherwise keep their stream
		// and decoder until the next PlayMusic or StopMusic.
		void Update();
		// How many tracks are still fading out, for tests.
		[[nodiscard]] std::size_t FadingTrackCount() const noexcept { return leaving.size(); }

		// Starts music from an Ogg Vorbis file, streamed from memory in small pieces on the audio thread, and looped.
		// Any music already playing fades out over the same time as the new one fades in (a crossfade); with none, the
		// new one just fades in. Asking for the music that is already playing does nothing. Returns false, with the
		// reason in `error`, if the file can't be played (the old music then carries on).
		bool PlayMusic(const std::filesystem::path& path, float fadeSeconds, std::string& error);
		// Fades the music out and stops it. A fade of 0 seconds stops it at once.
		void StopMusic(float fadeSeconds);
		[[nodiscard]] bool IsMusicPlaying() const noexcept;
	};
} // namespace engine::audio
