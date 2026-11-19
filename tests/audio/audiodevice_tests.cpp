#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#include "audio/audiodevice.hpp"
#include "audio/soundfile.hpp"

// The device's own tests need SDL, so they are platform tests. They run on SDL's dummy audio driver, which takes the
// samples and plays nothing, so the machine needs no sound card. What is heard is checked by ear (see docs/audio.md);
// the choices the mixer makes are tested without SDL in mixing_tests.cpp.
namespace
{
	using engine::audio::AudioDevice;
	using engine::audio::Bus;

	const std::filesystem::path kData{ENGINE_TEST_DATA_DIR};

	// Chosen before main, so SDL has not looked for an audio driver yet.
	[[maybe_unused]] const bool kDummyAudioChosen = []
	{
#ifdef _WIN32
		_putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
		setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
		return true;
	}();

	void Wait(int milliseconds)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
	}
} // namespace

TEST_CASE("The device plays sounds of any format and takes volume and priority", "[audio][device]")
{
	AudioDevice audio;
	if (!audio.IsAvailable()) SKIP("no audio device, not even the dummy driver");

	const auto wav = engine::audio::LoadSound(kData / "audio" / "tone_stereo_22050.wav");
	const auto ogg = engine::audio::LoadSound(kData / "audio" / "tone_mono_44100.ogg");
	REQUIRE(wav.Ok());
	REQUIRE(ogg.Ok());

	// More sounds than voices, on every bus, at different priorities: it must neither crash nor block.
	for (int i = 0; i < 100; ++i)
	{
		audio.Play(wav.sound, {.bus = Bus::Ui, .volume = 0.5f, .priority = i % 3});
		audio.Play(ogg.sound, {.bus = Bus::Sfx, .pitchVariance = 0.2f, .priority = i % 5});
		audio.Play(ogg.sound, {.bus = Bus::Music, .volume = 2.0f}); // out of range is clamped
	}
	audio.Play({}); // an empty Sound is nothing to play

	audio.SetBusVolume(Bus::Sfx, 0.25f);
	CHECK(audio.BusVolume(Bus::Sfx) == 0.25f);
	audio.SetBusVolume(Bus::Ui, -3.0f);
	CHECK(audio.BusVolume(Bus::Ui) == 0.0f);
	audio.SetMasterVolume(0.5f);
	CHECK(audio.MasterVolume() == 0.5f);
}

TEST_CASE("Music starts, crossfades to another track, stops, and a bad file leaves it alone", "[audio][device][music]")
{
	AudioDevice audio;
	if (!audio.IsAvailable()) SKIP("no audio device, not even the dummy driver");

	const std::filesystem::path first = kData / "audio" / "tone_mono_44100.ogg";
	const std::filesystem::path second = std::filesystem::temp_directory_path() / "engine_audiodevice_test_second.ogg";
	std::filesystem::copy_file(first, second, std::filesystem::copy_options::overwrite_existing);

	std::string error;
	CHECK_FALSE(audio.IsMusicPlaying());
	REQUIRE(audio.PlayMusic(first, 0.05f, error));
	CHECK(error.empty());
	CHECK(audio.IsMusicPlaying());
	CHECK(audio.PlayMusic(first, 0.05f, error)); // already playing: nothing to do

	Wait(30); // let the audio thread decode some

	REQUIRE(audio.PlayMusic(second, 0.05f, error)); // the crossfade
	Wait(100);

	CHECK_FALSE(audio.PlayMusic(kData / "audio" / "missing.ogg", 0.05f, error));
	CHECK_FALSE(error.empty());
	CHECK(audio.IsMusicPlaying()); // the track that was playing carries on

	audio.SetBusVolume(Bus::Music, 0.3f);
	audio.StopMusic(0.05f);
	CHECK_FALSE(audio.IsMusicPlaying());
	Wait(100);
	audio.StopMusic(0.0f); // nothing playing: harmless

	// The tracks that faded out are still held until Update lets them go.
	audio.Update();
	CHECK(audio.FadingTrackCount() == 0);

	std::error_code ec;
	std::filesystem::remove(second, ec);
}

TEST_CASE("Tracks that have faded out are released by Update, and fades requested in a hurry end where the last asked",
          "[audio][device][music]")
{
	AudioDevice audio;
	if (!audio.IsAvailable()) SKIP("no audio device, not even the dummy driver");

	const std::filesystem::path first = kData / "audio" / "tone_mono_44100.ogg";
	const std::filesystem::path second = std::filesystem::temp_directory_path() / "engine_audiodevice_test_fast.ogg";
	std::filesystem::copy_file(first, second, std::filesystem::copy_options::overwrite_existing);

	// Crossfades back and forth faster than the audio thread can follow each: every request is read whole.
	std::string error;
	for (int i = 0; i < 40; ++i)
	{
		REQUIRE(audio.PlayMusic(i % 2 == 0 ? first : second, 0.01f, error));
	}
	CHECK(audio.IsMusicPlaying());
	CHECK(audio.FadingTrackCount() > 0);

	Wait(200); // long enough for every fade-out to finish, and the loop to wrap round the short file a few times
	audio.Update();
	CHECK(audio.FadingTrackCount() == 0);
	CHECK(audio.IsMusicPlaying()); // the last one is still going, and never ran dry

	audio.StopMusic(0.02f);
	Wait(150);
	audio.Update();
	CHECK(audio.FadingTrackCount() == 0);
	CHECK_FALSE(audio.IsMusicPlaying());

	std::error_code ec;
	std::filesystem::remove(second, ec);
}
