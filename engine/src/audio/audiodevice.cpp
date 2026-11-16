#include "audio/audiodevice.hpp"

#include <SDL3/SDL.h>

#include "audio/sound.hpp"

namespace engine::audio
{
	AudioDevice::AudioDevice()
	{
		if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
		{
			SDL_Log("SDL_InitSubSystem(SDL_INIT_AUDIO) failed, playing without sound: %s", SDL_GetError());
			return;
		}
		subsystemStarted = true;

		const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, kSampleRate};
		device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
		if (device == 0)
		{
			SDL_Log("SDL_OpenAudioDevice failed, playing without sound: %s", SDL_GetError());
			return;
		}

		// One stream per voice, all bound to the device, which mixes them. Each converts our
		// format to the device's.
		for (SDL_AudioStream*& voice : voices)
		{
			voice = SDL_CreateAudioStream(&spec, &spec);
			if (voice == nullptr)
			{
				SDL_Log("SDL_CreateAudioStream failed: %s", SDL_GetError());
				continue;
			}
			if (!SDL_BindAudioStream(device, voice))
			{
				SDL_Log("SDL_BindAudioStream failed: %s", SDL_GetError());
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
		if (device == 0 || sound.samples.empty()) return;

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

		const auto bytes = static_cast<int>(sound.samples.size() * sizeof(float));
		if (!SDL_PutAudioStreamData(chosen, sound.samples.data(), bytes))
		{
			SDL_Log("SDL_PutAudioStreamData failed: %s", SDL_GetError());
		}
	}
} // namespace engine::audio
