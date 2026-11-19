#include "audio/soundfile.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <vector>

#include "vorbis.hpp"

namespace engine::audio
{
	namespace
	{
		SoundResult Failure(const std::filesystem::path& path, const std::string& why)
		{
			SoundResult result;
			result.error = path.string() + ": " + why;
			return result;
		}

		std::string LowerExtension(const std::filesystem::path& path)
		{
			std::string extension = path.extension().string();
			std::ranges::transform(extension, extension.begin(), [](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});
			return extension;
		}

		// The file's size, if it is a sound we are willing to read: not empty and not too big.
		bool CheckSize(const std::filesystem::path& path, std::uintmax_t& size, std::string& why)
		{
			std::error_code ec;
			size = std::filesystem::file_size(path, ec);
			if (ec)
			{
				why = "can't read the file (" + ec.message() + ")";
				return false;
			}
			if (size == 0)
			{
				why = "the file is empty";
				return false;
			}
			if (size > kMaxSoundFileBytes || size > static_cast<std::uintmax_t>(std::numeric_limits<int>::max()))
			{
				why = "the file is too big";
				return false;
			}
			return true;
		}

		bool ReadFile(const std::filesystem::path& path, std::vector<unsigned char>& bytes, std::string& why)
		{
			std::uintmax_t size = 0;
			if (!CheckSize(path, size, why)) return false;
			std::ifstream in(path, std::ios::binary);
			bytes.resize(static_cast<std::size_t>(size));
			if (!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
			{
				why = "can't read the file";
				return false;
			}
			return true;
		}

		SoundResult LoadWav(const std::filesystem::path& path)
		{
			std::uintmax_t size = 0;
			std::string why;
			if (!CheckSize(path, size, why)) return Failure(path, why);

			const auto utf8 = path.u8string();
			SDL_AudioSpec spec{};
			Uint8* data = nullptr;
			Uint32 length = 0;
			if (!SDL_LoadWAV(reinterpret_cast<const char*>(utf8.c_str()), &spec, &data, &length))
			{
				return Failure(path, std::string("not a readable WAV file: ") + SDL_GetError());
			}

			// To float, keeping the file's rate and channels.
			if (length == 0 || length > static_cast<Uint32>(std::numeric_limits<int>::max() / 2))
			{
				SDL_free(data);
				return Failure(path, length == 0 ? "the WAV file has no sound in it" : "the WAV file is too big");
			}
			SDL_AudioSpec wanted = spec;
			wanted.format = SDL_AUDIO_F32;
			Uint8* converted = nullptr;
			int convertedLength = 0;
			const bool ok =
			    SDL_ConvertAudioSamples(&spec, data, static_cast<int>(length), &wanted, &converted, &convertedLength);
			SDL_free(data);
			if (!ok || converted == nullptr)
			{
				return Failure(path, std::string("can't convert the WAV samples: ") + SDL_GetError());
			}

			SoundResult result;
			result.sound.sampleRate = spec.freq;
			result.sound.channels = spec.channels;
			result.sound.samples.resize(static_cast<std::size_t>(convertedLength) / sizeof(float));
			SDL_memcpy(result.sound.samples.data(), converted, result.sound.samples.size() * sizeof(float));
			SDL_free(converted);
			if (result.sound.samples.empty() || result.sound.channels < 1 || result.sound.sampleRate < 1)
			{
				return Failure(path, "the WAV file has no sound in it");
			}
			return result;
		}

		SoundResult LoadOgg(const std::filesystem::path& path)
		{
			std::vector<unsigned char> bytes;
			std::string why;
			if (!ReadFile(path, bytes, why)) return Failure(path, why);

			int channels = 0;
			int rate = 0;
			short* decoded = nullptr;
			const int frames =
			    stb_vorbis_decode_memory(bytes.data(), static_cast<int>(bytes.size()), &channels, &rate, &decoded);
			if (frames <= 0 || decoded == nullptr || channels < 1 || rate < 1)
			{
				std::free(decoded);
				return Failure(path, "not a readable Ogg Vorbis file");
			}

			SoundResult result;
			result.sound.sampleRate = rate;
			result.sound.channels = channels;
			const std::size_t count = static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
			result.sound.samples.resize(count);
			for (std::size_t i = 0; i < count; ++i)
			{
				result.sound.samples[i] = static_cast<float>(decoded[i]) / 32768.0f;
			}
			std::free(decoded);
			return result;
		}
	} // namespace

	SoundResult LoadSound(const std::filesystem::path& path)
	{
		const std::string extension = LowerExtension(path);
		if (extension == ".wav") return LoadWav(path);
		if (extension == ".ogg") return LoadOgg(path);
		return Failure(path, "unsupported sound file type (use .wav or .ogg)");
	}
} // namespace engine::audio
