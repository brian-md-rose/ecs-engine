#include "audio/music.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>

#include "audio/mixing.hpp"
#include "audio/soundfile.hpp"
#include "vorbis.hpp"

namespace engine::audio
{
	namespace
	{
		constexpr int kChunkFrames = 2048; // frames decoded at a time on the audio thread
	}

	std::unique_ptr<MusicTrack> MusicTrack::Open(std::uint32_t device, const std::filesystem::path& path,
	                                             std::string& error)
	{
		std::error_code ec;
		const std::uintmax_t size = std::filesystem::file_size(path, ec);
		if (ec || size == 0 || size > kMaxSoundFileBytes ||
		    size > static_cast<std::uintmax_t>(std::numeric_limits<int>::max()))
		{
			error = path.string() + ": can't read the music file";
			return nullptr;
		}

		std::unique_ptr<MusicTrack> track(new MusicTrack());
		track->file.resize(static_cast<std::size_t>(size));
		{
			std::ifstream in(path, std::ios::binary);
			if (!in.read(reinterpret_cast<char*>(track->file.data()), static_cast<std::streamsize>(track->file.size())))
			{
				error = path.string() + ": can't read the music file";
				return nullptr;
			}
		}

		int vorbisError = 0;
		track->decoder =
		    stb_vorbis_open_memory(track->file.data(), static_cast<int>(track->file.size()), &vorbisError, nullptr);
		if (track->decoder == nullptr)
		{
			error = path.string() + ": not a readable Ogg Vorbis file";
			return nullptr;
		}
		const stb_vorbis_info info = stb_vorbis_get_info(track->decoder);
		track->channels = info.channels;
		track->sampleRate = static_cast<int>(info.sample_rate);
		if (track->channels < 1 || track->channels > 8 || track->sampleRate < 1)
		{
			error = path.string() + ": the music file has an unsupported format";
			return nullptr; // ~MusicTrack closes the decoder
		}
		track->chunk.resize(static_cast<std::size_t>(kChunkFrames) * static_cast<std::size_t>(track->channels));

		SDL_AudioSpec output{};
		if (!SDL_GetAudioDeviceFormat(device, &output, nullptr))
		{
			error = std::string("no audio device format: ") + SDL_GetError();
			return nullptr;
		}
		const SDL_AudioSpec input{SDL_AUDIO_F32, track->channels, track->sampleRate};
		track->stream = SDL_CreateAudioStream(&input, &output);
		if (track->stream == nullptr)
		{
			error = std::string("SDL_CreateAudioStream failed: ") + SDL_GetError();
			return nullptr;
		}
		// The level starts at 0 and only a fade raises it, so a new track never starts at full volume for a moment.
		if (!SDL_SetAudioStreamGetCallback(track->stream, &MusicTrack::OnStreamNeedsData, track.get()) ||
		    !SDL_BindAudioStream(device, track->stream))
		{
			error = std::string("can't start the music stream: ") + SDL_GetError();
			return nullptr;
		}
		return track;
	}

	MusicTrack::~MusicTrack()
	{
		// The stream first: destroying it waits for the audio thread to leave the callback, which uses the decoder.
		if (stream != nullptr) SDL_DestroyAudioStream(stream);
		if (decoder != nullptr) stb_vorbis_close(decoder);
	}

	void MusicTrack::FadeTo(float target, double seconds)
	{
		const double frames = std::max(0.0, std::isfinite(seconds) ? seconds : 0.0) * sampleRate;
		requestTarget.store(ClampVolume(target), std::memory_order_relaxed);
		requestFrames.store(static_cast<std::uint64_t>(frames), std::memory_order_relaxed);
		requestSerial.fetch_add(1, std::memory_order_release); // last: it says the rest is ready
	}

	void MusicTrack::SetVolume(float volume)
	{
		if (stream != nullptr) SDL_SetAudioStreamGain(stream, ClampVolume(volume));
	}

	void MusicTrack::OnStreamNeedsData(void* userdata, SDL_AudioStream*, int additional, int) noexcept
	{
		static_cast<MusicTrack*>(userdata)->Fill(additional);
	}

	// Runs on SDL's audio thread. It touches only the audio thread's own members and the atomics.
	void MusicTrack::Fill(int additionalBytes) noexcept
	{
		if (silent.load(std::memory_order_relaxed) || additionalBytes <= 0) return;

		const auto frameBytes = static_cast<int>(sizeof(float)) * channels;
		int framesWanted = additionalBytes / frameBytes;
		while (framesWanted > 0)
		{
			// A new fade request starts from wherever the level is now.
			const std::uint32_t serial = requestSerial.load(std::memory_order_acquire);
			if (serial != seenSerial)
			{
				seenSerial = serial;
				fadeFrom = gain;
				fadeTo = requestTarget.load(std::memory_order_relaxed);
				fadeTotal = requestFrames.load(std::memory_order_relaxed);
				fadeElapsed = 0;
			}

			// Decode, going back to the start when the file ends, so a loop has no gap.
			const int want = std::min(framesWanted, kChunkFrames);
			int filled = 0;
			int restarts = 0;
			while (filled < want)
			{
				const int got = stb_vorbis_get_samples_float_interleaved(
				    decoder, channels,
				    chunk.data() + static_cast<std::size_t>(filled) * static_cast<std::size_t>(channels),
				    (want - filled) * channels);
				if (got > 0)
				{
					filled += got;
					restarts = 0;
					continue;
				}
				if (++restarts > 1 || !stb_vorbis_seek_start(decoder))
				{
					// A file that gives nothing even from its start: stop rather than spin.
					std::fill(chunk.begin() + static_cast<std::ptrdiff_t>(filled) * channels, chunk.end(), 0.0f);
					silent.store(true, std::memory_order_relaxed);
					filled = want;
				}
			}

			for (int f = 0; f < want; ++f)
			{
				gain = EqualPowerFade(fadeFrom, fadeTo, fadeElapsed, fadeTotal);
				if (fadeElapsed < fadeTotal) ++fadeElapsed;
				for (int c = 0; c < channels; ++c)
				{
					chunk[static_cast<std::size_t>(f) * static_cast<std::size_t>(channels) +
					      static_cast<std::size_t>(c)] *= gain;
				}
			}
			SDL_PutAudioStreamData(stream, chunk.data(), want * frameBytes);
			framesWanted -= want;

			if (fadeTo == 0.0f && fadeElapsed >= fadeTotal && gain == 0.0f && seenSerial != 0)
			{
				silent.store(true, std::memory_order_relaxed); // faded out: nothing more to play
				return;
			}
			if (silent.load(std::memory_order_relaxed)) return;
		}
	}
} // namespace engine::audio
