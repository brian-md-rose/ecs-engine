#include "audio/audiodevice.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <limits>

#include "audio/mixing.hpp"
#include "audio/music.hpp"
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
		for (Voice& voice : voices)
		{
			voice.stream = SDL_CreateAudioStream(&input, &spec);
			if (voice.stream == nullptr)
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateAudioStream failed: %s", SDL_GetError());
				continue;
			}
			if (!SDL_BindAudioStream(device, voice.stream))
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_BindAudioStream failed: %s", SDL_GetError());
				SDL_DestroyAudioStream(voice.stream);
				voice.stream = nullptr;
			}
		}
	}

	AudioDevice::~AudioDevice()
	{
		// Music first: its streams call back into the decoder until they are destroyed.
		leaving.clear();
		music.reset();
		for (const Voice& voice : voices)
		{
			if (voice.stream != nullptr) SDL_DestroyAudioStream(voice.stream); // unbinds it too
		}
		if (device != 0) SDL_CloseAudioDevice(device);
		if (subsystemStarted) SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}

	bool AudioDevice::IsBusy(const Voice& voice) const
	{
		return voice.stream != nullptr && SDL_GetAudioStreamQueued(voice.stream) > 0;
	}

	void AudioDevice::ApplyGain(const Voice& voice) const
	{
		const float gain = VoiceGain(voice.volume, busVolumes[static_cast<std::size_t>(voice.bus)], masterVolume);
		SDL_SetAudioStreamGain(voice.stream, gain);
	}

	void AudioDevice::ApplyMusicVolume() const
	{
		const float volume = VoiceGain(1.0f, busVolumes[static_cast<std::size_t>(Bus::Music)], masterVolume);
		if (music) music->SetVolume(volume);
		for (const auto& track : leaving)
			track->SetVolume(volume);
	}

	float AudioDevice::NextRandom()
	{
		// xorshift32: plenty for picking a pitch.
		randomState ^= randomState << 13;
		randomState ^= randomState >> 17;
		randomState ^= randomState << 5;
		return static_cast<float>(randomState >> 8) / static_cast<float>(1u << 24);
	}

	void AudioDevice::Play(const Sound& sound, const PlayOptions& options)
	{
		if (device == 0 || sound.samples.empty() || sound.sampleRate < 1 || sound.channels < 1) return;
		const auto busIndex = static_cast<std::size_t>(options.bus);
		if (busIndex >= kBusCount) return;

		// Which voice: a free one, or else the oldest of the least important, if the sound matters as much as it.
		std::array<VoiceState, kVoices> states{};
		for (std::size_t i = 0; i < kVoices; ++i)
		{
			// A voice whose stream could not be made counts as busy at the top priority, so it is never picked.
			const bool usable = voices[i].stream != nullptr;
			states[i] = {.busy = !usable || IsBusy(voices[i]),
			             .priority = usable ? voices[i].priority : std::numeric_limits<int>::max(),
			             .serial = voices[i].serial};
		}
		const std::size_t index = ChooseVoice(states, options.priority);
		if (index >= kVoices) return;
		Voice& voice = voices[index];
		if (IsBusy(voice)) SDL_ClearAudioStream(voice.stream);

		// The stream takes the sound's own format and SDL resamples and remixes it for the device.
		const SDL_AudioSpec input{SDL_AUDIO_F32, sound.channels, sound.sampleRate};
		if (!SDL_SetAudioStreamFormat(voice.stream, &input, nullptr))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_SetAudioStreamFormat failed: %s", SDL_GetError());
			return;
		}
		const std::size_t byteCount = sound.samples.size() * sizeof(float);
		if (byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return; // too big to hand SDL at once
		voice.bus = options.bus;
		voice.priority = options.priority;
		voice.serial = nextSerial++;
		voice.volume = ClampVolume(options.volume);
		ApplyGain(voice);
		const float ratio = options.pitchVariance > 0.0f ? PitchRatio(options.pitchVariance, NextRandom()) : 1.0f;
		SDL_SetAudioStreamFrequencyRatio(voice.stream, ratio);

		if (!SDL_PutAudioStreamData(voice.stream, sound.samples.data(), static_cast<int>(byteCount)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_PutAudioStreamData failed: %s", SDL_GetError());
		}
	}

	void AudioDevice::SetBusVolume(Bus bus, float volume)
	{
		const auto index = static_cast<std::size_t>(bus);
		if (index >= kBusCount) return;
		busVolumes[index] = ClampVolume(volume);
		if (device == 0) return;
		for (const Voice& voice : voices)
		{
			if (voice.stream != nullptr && voice.bus == bus) ApplyGain(voice);
		}
		if (bus == Bus::Music) ApplyMusicVolume();
	}

	float AudioDevice::BusVolume(Bus bus) const noexcept
	{
		const auto index = static_cast<std::size_t>(bus);
		return index < kBusCount ? busVolumes[index] : 0.0f;
	}

	void AudioDevice::SetMasterVolume(float volume)
	{
		masterVolume = ClampVolume(volume);
		if (device == 0) return;
		for (const Voice& voice : voices)
		{
			if (voice.stream != nullptr) ApplyGain(voice);
		}
		ApplyMusicVolume();
	}

	void AudioDevice::ReapMusic()
	{
		std::erase_if(leaving, [](const std::unique_ptr<MusicTrack>& track)
		{
			return track->IsSilent();
		});
	}

	bool AudioDevice::PlayMusic(const std::filesystem::path& path, float fadeSeconds, std::string& error)
	{
		error.clear();
		if (device == 0) return false;
		ReapMusic();
		if (music && path == musicPath) return true; // already playing

		std::unique_ptr<MusicTrack> next = MusicTrack::Open(device, path, error);
		if (!next) return false;

		// The crossfade: the old track out and the new one in, over the same time.
		if (music)
		{
			music->FadeTo(0.0f, fadeSeconds);
			leaving.push_back(std::move(music));
		}
		music = std::move(next);
		musicPath = path;
		ApplyMusicVolume();
		music->FadeTo(1.0f, fadeSeconds);
		return true;
	}

	void AudioDevice::StopMusic(float fadeSeconds)
	{
		if (device == 0 || !music) return;
		if (fadeSeconds <= 0.0f)
		{
			music.reset();
		}
		else
		{
			music->FadeTo(0.0f, fadeSeconds);
			leaving.push_back(std::move(music));
		}
		musicPath.clear();
		ReapMusic();
	}
} // namespace engine::audio
