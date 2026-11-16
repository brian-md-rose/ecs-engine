#include "sounds.hpp"

#include "audio/audiodevice.hpp"
#include "options.hpp"

namespace game
{
	namespace
	{
		using engine::audio::Tone;
		using engine::audio::Waveform;

		// A short, soft tick, high and thin, so a run of pellets patters rather than drones.
		constexpr Tone kEat[] = {
		    {.wave = Waveform::Square,
		     .startHz = 1180.0f,
		     .endHz = 1500.0f,
		     .seconds = 0.045f,
		     .volume = 0.10f,
		     .attackSeconds = 0.002f,
		     .releaseSeconds = 0.02f,
		     .duty = 0.25f},
		};

		// A bright rising swoop with a shimmer on top: something just changed in the player's favour.
		constexpr Tone kPowerUp[] = {
		    {.wave = Waveform::Triangle, .startHz = 220.0f, .endHz = 990.0f, .seconds = 0.28f, .volume = 0.30f},
		    {.wave = Waveform::Square,
		     .startHz = 990.0f,
		     .endHz = 1320.0f,
		     .seconds = 0.12f,
		     .volume = 0.12f,
		     .duty = 0.125f},
		};

		// Three quick climbing blips, like a coin counter.
		constexpr Tone kEnemyEaten[] = {
		    {.wave = Waveform::Square, .startHz = 523.0f, .endHz = 523.0f, .seconds = 0.06f, .volume = 0.16f},
		    {.wave = Waveform::Square, .startHz = 784.0f, .endHz = 784.0f, .seconds = 0.06f, .volume = 0.16f},
		    {.wave = Waveform::Square, .startHz = 1175.0f, .endHz = 1400.0f, .seconds = 0.12f, .volume = 0.16f},
		};

		// A long falling wail that crumbles into static.
		constexpr Tone kDeath[] = {
		    {.wave = Waveform::Square,
		     .startHz = 660.0f,
		     .endHz = 110.0f,
		     .seconds = 0.8f,
		     .volume = 0.20f,
		     .releaseSeconds = 0.1f,
		     .duty = 0.4f},
		    {.wave = Waveform::Noise,
		     .startHz = 2400.0f,
		     .endHz = 300.0f,
		     .seconds = 0.35f,
		     .volume = 0.18f,
		     .releaseSeconds = 0.25f},
		};

		// A little four-note fanfare, rising, with the last note held.
		constexpr Tone kLevelClear[] = {
		    {.wave = Waveform::Triangle, .startHz = 392.0f, .endHz = 392.0f, .seconds = 0.11f, .volume = 0.32f},
		    {.wave = Waveform::Triangle, .startHz = 494.0f, .endHz = 494.0f, .seconds = 0.11f, .volume = 0.32f},
		    {.wave = Waveform::Triangle, .startHz = 587.0f, .endHz = 587.0f, .seconds = 0.11f, .volume = 0.32f},
		    {.wave = Waveform::Triangle,
		     .startHz = 784.0f,
		     .endHz = 784.0f,
		     .seconds = 0.45f,
		     .volume = 0.32f,
		     .releaseSeconds = 0.3f},
		};
	} // namespace

	std::span<const Tone> TonesFor(Sfx sfx) noexcept
	{
		switch (sfx)
		{
		case Sfx::Eat:
			return kEat;
		case Sfx::PowerUp:
			return kPowerUp;
		case Sfx::EnemyEaten:
			return kEnemyEaten;
		case Sfx::Death:
			return kDeath;
		case Sfx::LevelClear:
			return kLevelClear;
		case Sfx::Count:
			break;
		}
		return {};
	}

	SoundBank::SoundBank()
	{
		for (std::size_t i = 0; i < kSfxCount; ++i)
		{
			sounds[i] = engine::audio::Synthesize(TonesFor(static_cast<Sfx>(i)));
		}
	}

	void AudioSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context)
	{
		SoundQueue& queue = world.resources.Get<SoundQueue>();
		const SoundBank& bank = world.resources.Get<SoundBank>();
		if (!world.resources.Get<Settings>().sound)
		{
			queue.pending.clear(); // muted in the options: nothing plays, so nothing counts as played
			return;
		}
		std::array<bool, kSfxCount> started{};
		for (const Sfx sfx : queue.pending)
		{
			const auto index = static_cast<std::size_t>(sfx);
			if (started[index]) continue; // two enemies catching the player at once is one death
			started[index] = true;
			if (context.audio != nullptr) context.audio->Play(bank.Get(sfx));
			++queue.played[index];
		}
		queue.pending.clear();
	}
} // namespace game
