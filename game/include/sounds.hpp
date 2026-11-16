#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "audio/sound.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "events.hpp"

// The game's sound effects: which event makes which sound, and what each sounds like. Every
// sound is synthesised from the tones in sounds.cpp when the game starts; there are no sound
// files. Events are turned into sounds by handlers that only queue them (testable without SDL),
// and AudioSystem plays the queue on the device at the end of each update.
namespace game
{
	enum class Sfx : std::uint8_t
	{
		Eat,        // a pellet
		PowerUp,    // a power pellet
		EnemyEaten, // a frightened enemy
		Death,      // caught
		LevelClear, // the last pellet
		Count
	};

	inline constexpr std::size_t kSfxCount = static_cast<std::size_t>(Sfx::Count);

	// Which sound each event makes.
	[[nodiscard]] constexpr Sfx SoundFor(const PelletEaten&) noexcept
	{
		return Sfx::Eat;
	}
	[[nodiscard]] constexpr Sfx SoundFor(const PowerPelletEaten&) noexcept
	{
		return Sfx::PowerUp;
	}
	[[nodiscard]] constexpr Sfx SoundFor(const EnemyEaten&) noexcept
	{
		return Sfx::EnemyEaten;
	}
	[[nodiscard]] constexpr Sfx SoundFor(const PlayerCaught&) noexcept
	{
		return Sfx::Death;
	}
	[[nodiscard]] constexpr Sfx SoundFor(const LevelCleared&) noexcept
	{
		return Sfx::LevelClear;
	}

	// The tones a sound is made of (see engine::audio::Tone). Our own designs.
	[[nodiscard]] std::span<const engine::audio::Tone> TonesFor(Sfx sfx) noexcept;

	// Resource: every sound, synthesised once at setup.
	struct SoundBank
	{
		std::array<engine::audio::Sound, kSfxCount> sounds;

		SoundBank();
		[[nodiscard]] const engine::audio::Sound& Get(Sfx sfx) const noexcept
		{
			return sounds[static_cast<std::size_t>(sfx)];
		}
	};

	// Resource: sounds asked for this update, and how many of each have been played, which the
	// tests read.
	struct SoundQueue
	{
		std::vector<Sfx> pending;
		std::array<int, kSfxCount> played{};

		[[nodiscard]] int Played(Sfx sfx) const noexcept { return played[static_cast<std::size_t>(sfx)]; }
	};

	// Event handler: queues the event's sound (SoundFor). MazeGame subscribes it last for each
	// event, so the sound follows whatever the event changed.
	template <typename E> void QueueSound(engine::ecs::World& world, const E& event)
	{
		world.resources.Get<SoundQueue>().pending.push_back(SoundFor(event));
	}

	// LateUpdate (last, always): plays every queued sound on context.audio, if there is one and the
	// Settings have sound on (otherwise the queue is just emptied),
	// counts it as played, and empties the queue. A sound queued twice in one update plays once.
	void AudioSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
