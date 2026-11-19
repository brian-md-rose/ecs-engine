#include "audio/audiodevice.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <limits>

#include "audio/sound.hpp"

namespace engine::audio
{
	AudioDevice::AudioDevice()
	{
		if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
			            "SDL_InitSubSystem(SDL_INIT_AUDIO) failed, playing without sound: %s", SDL_GetError());
			return;
		}
		subsystemStarted = true;

		// Stereo, so a stereo file keeps its sides; a mono sound is played on both.
		const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, kSampleRate};
		device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
		if (device == 0)
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_OpenAudioDevice failed, playing without sound: %s",
			            SDL_GetError());
			return;
		}

		// One stream per voice, all bound to the device, which mixes them. Each converts what it is
		// given (see Play) to the device's format.
		const SDL_AudioSpec input{SDL_AUDIO_F32, 1, kSampleRate};
		for (SDL_AudioStream*& voice : voices)
		{
			voice = SDL_CreateAudioStream(&input, &spec);
			if (voice == nullptr)
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateAudioStream failed: %s", SDL_GetError());
				continue;
			}
			if (!SDL_BindAudioStream(device, voice))
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_BindAudioStream failed: %s", SDL_GetError());
				SDL_DestroyAudioStream(voice);
				voice = nullptr;
			}
		}
	}

	AudioDevice::~AudioDevice()
	{
		for (SDL_AudioStream* voice : voices)
		{
			if (voice != nullptr) SDL_DestroyAudioStream(voice); // unbinds it too
		}
		if (device != 0) SDL_CloseAudioDevice(device);
		if (subsystemStarted) SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}

	void AudioDevice::Play(const Sound& sound)
	{
		if (device == 0 || sound.samples.empty() || sound.sampleRate < 1 || sound.channels < 1) return;

		// A voice that has finished, or else the next in turn (the one started longest ago).
		SDL_AudioStream* chosen = nullptr;
		for (SDL_AudioStream* voice : voices)
		{
			if (voice != nullptr && SDL_GetAudioStreamQueued(voice) == 0)
			{
				chosen = voice;
				break;
			}
		}
		if (chosen == nullptr)
		{
			chosen = voices[nextVoice];
			nextVoice = (nextVoice + 1) % kVoices;
			if (chosen == nullptr) return;
			SDL_ClearAudioStream(chosen);
		}

		// The stream takes the sound's own format and SDL resamples and remixes it for the device.
		const SDL_AudioSpec input{SDL_AUDIO_F32, sound.channels, sound.sampleRate};
		if (!SDL_SetAudioStreamFormat(chosen, &input, nullptr))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_SetAudioStreamFormat failed: %s", SDL_GetError());
			return;
		}
		const std::size_t byteCount = sound.samples.size() * sizeof(float);
		if (byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return; // too big for SDL to take at once
		const auto bytes = static_cast<int>(byteCount);
		if (!SDL_PutAudioStreamData(chosen, sound.samples.data(), bytes))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_PutAudioStreamData failed: %s", SDL_GetError());
		}
	}
} // namespace engine::audio
