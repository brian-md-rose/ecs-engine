#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

#include "audio/soundfile.hpp"

namespace
{
	using engine::audio::LoadSound;

	const std::filesystem::path kData{ENGINE_TEST_DATA_DIR};

	// A file in the temp folder with the given bytes, removed when it goes out of scope.
	struct TempFile
	{
		std::filesystem::path path;

		TempFile(const std::string& name, const std::string& bytes)
		    : path(std::filesystem::temp_directory_path() / ("engine_soundfile_test_" + name))
		{
			std::ofstream(path, std::ios::binary) << bytes;
		}
		~TempFile()
		{
			std::error_code ec;
			std::filesystem::remove(path, ec);
		}
		TempFile(const TempFile&) = delete;
		TempFile& operator=(const TempFile&) = delete;
	};

	float Peak(const engine::audio::Sound& sound)
	{
		float peak = 0.0f;
		for (const float sample : sound.samples)
			peak = std::max(peak, std::abs(sample));
		return peak;
	}
} // namespace

TEST_CASE("A WAV keeps its own sample rate and channels", "[audio][soundfile]")
{
	const auto result = LoadSound(kData / "audio" / "tone_stereo_22050.wav");
	REQUIRE(result.Ok());
	CHECK(result.sound.sampleRate == 22050);
	CHECK(result.sound.channels == 2);
	CHECK(result.sound.Seconds() > 0.09);
	CHECK(result.sound.Seconds() < 0.11);
	CHECK(Peak(result.sound) > 0.05f); // there is sound in it
	CHECK(Peak(result.sound) <= 1.0f);
}

TEST_CASE("An OGG keeps its own sample rate and channels", "[audio][soundfile]")
{
	const auto result = LoadSound(kData / "audio" / "tone_mono_44100.ogg");
	REQUIRE(result.Ok());
	CHECK(result.sound.sampleRate == 44100);
	CHECK(result.sound.channels == 1);
	CHECK(result.sound.Seconds() > 0.09);
	CHECK(result.sound.Seconds() < 0.13); // codecs pad the end a little
	CHECK(Peak(result.sound) > 0.05f);
	CHECK(Peak(result.sound) <= 1.0f);
}

TEST_CASE("The file type comes from the extension, in any case", "[audio][soundfile]")
{
	// A copy of the WAV under a different case.
	std::ifstream in(kData / "audio" / "tone_stereo_22050.wav", std::ios::binary);
	const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	REQUIRE_FALSE(bytes.empty());
	const TempFile upper("upper.WAV", bytes);
	CHECK(LoadSound(upper.path).Ok());

	const TempFile other("sound.mp3", bytes);
	const auto result = LoadSound(other.path);
	CHECK_FALSE(result.Ok());
	CHECK(result.error.find("unsupported") != std::string::npos);
}

TEST_CASE("A bad file is an error, not a crash", "[audio][soundfile]")
{
	SECTION("a file that isn't there")
	{
		const auto result = LoadSound(kData / "audio" / "missing.wav");
		CHECK_FALSE(result.Ok());
		CHECK(result.sound.samples.empty());
	}
	SECTION("an empty file")
	{
		const TempFile wav("empty.wav", "");
		const TempFile ogg("empty.ogg", "");
		CHECK_FALSE(LoadSound(wav.path).Ok());
		CHECK_FALSE(LoadSound(ogg.path).Ok());
	}
	SECTION("text called a WAV or an OGG")
	{
		const TempFile wav("text.wav", "this is not a RIFF file, just words");
		const TempFile ogg("text.ogg", "this is not an Ogg file either, just words");
		CHECK_FALSE(LoadSound(wav.path).Ok());
		CHECK_FALSE(LoadSound(ogg.path).Ok());
	}
	SECTION("a WAV with a good header and no sound")
	{
		// RIFF header, 16-bit mono at 8 kHz, and a data chunk of zero bytes.
		const std::string header("RIFF\x24\x00\x00\x00WAVEfmt \x10\x00\x00\x00\x01\x00\x01\x00\x40\x1f\x00\x00"
		                         "\x80\x3e\x00\x00\x02\x00\x10\x00"
		                         "data\x00\x00\x00\x00",
		                         44);
		const TempFile silent("nodata.wav", header);
		const auto result = LoadSound(silent.path);
		CHECK_FALSE(result.Ok());
		CHECK(result.sound.samples.empty());
	}
	SECTION("a real file cut short")
	{
		std::ifstream in(kData / "audio" / "tone_mono_44100.ogg", std::ios::binary);
		std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		REQUIRE(bytes.size() > 100);
		bytes.resize(40); // inside the first header
		const TempFile cut("cut.ogg", bytes);
		CHECK_FALSE(LoadSound(cut.path).Ok());
	}
	SECTION("an unknown type")
	{
		const auto result = LoadSound("sound.flac");
		CHECK_FALSE(result.Ok());
		CHECK(result.error.find("unsupported") != std::string::npos);
	}
}

TEST_CASE("A sound that would unpack to more than the cap is refused before it is decoded", "[audio][soundfile]")
{
	using engine::audio::DecodedSizeAllowed;
	CHECK(DecodedSizeAllowed(44100ull * 60, 2)); // a minute of stereo
	CHECK(DecodedSizeAllowed(0, 1));
	CHECK_FALSE(DecodedSizeAllowed(1, 0));
	// The cap is on the float samples: 64 Mi of them at most.
	constexpr std::uint64_t kSamples = engine::audio::kMaxDecodedSoundBytes / sizeof(float);
	CHECK(DecodedSizeAllowed(kSamples / 2, 2));
	CHECK_FALSE(DecodedSizeAllowed(kSamples / 2 + 1, 2));
	CHECK_FALSE(DecodedSizeAllowed(0xffffffffull, 8)); // 4 billion frames of 8 channels
}
