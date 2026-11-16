#pragma once
#include <cstdint>

#include "ecs/states.hpp"

// The game's flow, built on the engine's states (ecs/states.hpp). Screen is the state machine
// proper; Round and Pause are sub-states of Screen::Playing.
//
//   Title ──START──► Playing ──(no lives left, new high score)──► NameEntry ──► GameOver ──► Title
//     │                │  │                                                         ▲
//     │                │  └──(no lives left)────────────────────────────────────────┘
//     │                └──(LevelCleared)──► LevelTransition ──(kLevelTransitionSeconds)──► Playing
//     └──OPTIONS──► Options ──ESC──► Title
//
//   Round, inside Playing:  Ready ──(kReadySeconds)──► Active ──(PlayerCaught)──► Dying
//                             ▲                                                     │
//                             └────────────────(kDyingSeconds, lives left)──────────┘
//   Pause, inside Playing:  Running ◄──► Paused (quit from the pause menu: back to the Title)
//
// Systems ask for a change through NextState<T>; it takes effect at the start of the next
// update (see ecs/states.hpp). What each screen sets up and takes down is in gamestates.cpp.
namespace game
{
	// Where the game is. The title, options and name-entry screens, the game itself, the pause
	// before the next level, and the end.
	enum class Screen : std::uint8_t
	{
		Title,
		Options,
		NameEntry,       // after a game that beat the high score
		Playing,         // a round is under way (see Round and Pause)
		LevelTransition, // the level was cleared: its number and the score, for a moment
		GameOver
	};

	// What the round is doing, while Playing.
	enum class Round : std::uint8_t
	{
		Ready,  // about to start; nothing moves
		Active, // the player moves, eats and can be caught
		Dying   // the player was caught; nothing moves
	};

	// Whether the game is stopped, while Playing.
	enum class Pause : std::uint8_t
	{
		Running,
		Paused
	};

	inline constexpr float kReadySeconds = 2.0f;
	inline constexpr float kDyingSeconds = 1.5f;
	inline constexpr float kLevelTransitionSeconds = 2.5f;
	inline constexpr int kStartingLives = 3; // until the options say otherwise

	// Resource: the game in progress.
	struct GameFlow
	{
		int lives = kStartingLives; // tries left, counting the current one
		int level = 1;
		float seconds = 0.0f; // time in the current timed state: Ready, Dying or LevelTransition
	};

	// Resource: whether the game's entities (maze, pellets, player, enemies, HUD) exist. They are
	// created on entering Playing, if not already there (SpawnGameWorld), so coming back from a
	// LevelTransition keeps the world. Only entering the Title removes them and clears this.
	// Invariant: every way back to Playing from the end of a game goes through the Title. A future
	// shortcut (a "Retry" from GameOver) must clear this too, or it would reuse an eaten maze.
	struct GameWorld
	{
		bool spawned = false;
	};

	// Every entity of the game's world carries this: it is destroyed on entering the Title.
	[[nodiscard]] inline engine::ecs::DespawnOnEnter<Screen> WorldScope() noexcept
	{
		return {Screen::Title};
	}

	// True once Ready or Dying has lasted its time (the other rounds have no timer).
	[[nodiscard]] bool RoundTimeUp(Round round, float seconds) noexcept;

	// Where a game that has just ended (the last life lost, or abandoned) goes: to the name
	// entry if its score beat the record, else to GameOver.
	[[nodiscard]] Screen ScreenAfterGame(int score, int record) noexcept;
} // namespace game
