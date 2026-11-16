#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// Only forward-declared: see graphics/renderer.hpp. Only engine .cpp files include <SDL3/...>.
struct SDL_AudioStream;

namespace engine::audio
{
	struct Sound;

	// Owns SDL's audio subsystem and the default playback device, and plays Sounds on it, several
	// at once. If there is no device (a machine without sound, a CI runner), the reason is logged
	// once and Play does nothing, so a game runs the same with or without sound.
	class AudioDevice
	{
		static constexpr std::size_t kVoices = 8; // sounds that can play at the same time

		std::uint32_t device = 0; // SDL_AudioDeviceID; 0 when there is none
		std::array<SDL_AudioStream*, kVoices> voices{};
		std::size_t nextVoice = 0;
		bool subsystemStarted = false;

	public:
		// Opens the default playback device. Needs SDL started (Application's SdlScope).
		AudioDevice();
		~AudioDevice();

		AudioDevice(const AudioDevice&) = delete;
		AudioDevice& operator=(const AudioDevice&) = delete;
		AudioDevice(AudioDevice&&) = delete;
		AudioDevice& operator=(AudioDevice&&) = delete;

		// False if no device could be opened; Play is then silent.
		[[nodiscard]] bool IsAvailable() const noexcept { return device != 0; }

		// Starts sound now, alongside whatever is playing. With every voice busy, one is cut off
		// for it, taking the voices in turn. The samples are copied, so sound needn't outlive the
		// call.
		void Play(const Sound& sound);
	};
} // namespace engine::audio
