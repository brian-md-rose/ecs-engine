#pragma once
#include <cstdint>

// The game's state machine as pure functions over a GameFlow value: no registry, no SDL.
// GameFlowSystem and the event handlers in systems.hpp apply them to the GameFlow resource.
//
//   Ready ──(kReadySeconds)──► Playing ──(PlayerCaught)──► Dying ──(kDyingSeconds)──► Ready
//                                 │                          └── no lives left ──► GameOver
//                                 └──(LevelCleared)──► LevelComplete ──(kLevelCompleteSeconds)──► Ready, next level
//   GameOver ──(Confirm)──► Ready, new game
namespace game
{
	enum class Stage : std::uint8_t
	{
		Ready,         // the round is about to start; nothing moves
		Playing,       // the player moves, eats and can be caught
		Dying,         // the player was caught; nothing moves
		LevelComplete, // every pellet is gone; nothing moves
		GameOver       // no lives left; waits for Confirm
	};

	inline constexpr float kReadySeconds = 2.0f;
	inline constexpr float kDyingSeconds = 1.5f;
	inline constexpr float kLevelCompleteSeconds = 2.5f;
	inline constexpr int kStartingLives = 3;

	// Resource: where the game is in its state machine.
	struct GameFlow
	{
		Stage stage = Stage::Ready;
		float stageSeconds = 0.0f;  // time spent in stage so far
		int lives = kStartingLives; // tries left, counting the current one
		int level = 1;
	};

	// Adds seconds to the time in the current stage and takes the timed transitions: Ready to
	// Playing, Dying to Ready (or to GameOver once no lives are left), and LevelComplete to
	// Ready on the next level. Entering a stage starts its clock at 0. Playing and GameOver
	// have no timer.
	[[nodiscard]] GameFlow Tick(GameFlow flow, float seconds) noexcept;

	// While Playing: one life fewer, and Dying. In any other stage the player can't be caught,
	// so the flow is unchanged.
	[[nodiscard]] GameFlow CatchPlayer(GameFlow flow) noexcept;

	// While Playing: LevelComplete. Unchanged in any other stage.
	[[nodiscard]] GameFlow ClearLevel(GameFlow flow) noexcept;

	// At GameOver: a new game, Ready on level 1 with every life back. Unchanged otherwise.
	[[nodiscard]] GameFlow Confirm(GameFlow flow) noexcept;

	// What to put back when the game goes from one stage to another.
	struct RoundReset
	{
		bool pellets = false; // respawn every pellet and power pellet
		bool actors = false;  // move the player (and later the enemies) back to their spawns
		bool score = false;   // start the score from 0
	};

	// Entering Ready puts the actors back. After LevelComplete the pellets come back too, and
	// after GameOver the score is reset as well. Nothing else resets anything.
	[[nodiscard]] RoundReset ResetFor(Stage from, Stage to) noexcept;
} // namespace game
