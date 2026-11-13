#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "input/keyboard.hpp"

namespace game
{
	// What the maze game asks about. Several keys can be bound to one action.
	enum class Action : std::uint8_t
	{
		MoveUp,
		MoveDown,
		MoveLeft,
		MoveRight,
		Pause,
		Confirm,
		Count
	};

	enum class Direction : std::uint8_t
	{
		None,
		Up,
		Down,
		Left,
		Right
	};

	inline constexpr std::size_t kActionCount = static_cast<std::size_t>(Action::Count);

	// Maps engine keys to game actions. Call Update() once at the start of each game update,
	// then query actions for the rest of it.
	class Controls
	{
		std::array<std::optional<Action>, engine::input::kKeyCount> bindings{};
		std::array<bool, kActionCount> held{};
		std::array<bool, kActionCount> pressed{};
		std::array<bool, kActionCount> released{};
		Direction bufferedDirection = Direction::None;

	public:
		// Starts with the default bindings: arrows and WASD to move, P or Escape to pause,
		// Enter or Space to confirm.
		Controls();

		// Binds key to action, replacing any previous binding of that key. Safe while keys are
		// held: action state is recomputed from the keyboard on the next Update().
		void Bind(engine::input::Key key, Action action);
		void Unbind(engine::input::Key key);
		[[nodiscard]] std::optional<Action> GetBinding(engine::input::Key key) const;

		void Update(const engine::input::Keyboard& keyboard);

		// At least one bound key is down.
		[[nodiscard]] bool IsHeld(Action action) const;
		// Went from not held to held since the last update (including a tap that is already
		// over). Pressing a second key for an action that is already held doesn't count.
		[[nodiscard]] bool WasPressed(Action action) const;
		// Went from held to not held since the last update.
		[[nodiscard]] bool WasReleased(Action action) const;

		// The most recent direction pressed, kept after the key is released. Grid movement
		// uses it to take a turn the player asked for a little before reaching the junction.
		// If two directions are first pressed within the same update, the later one in Action
		// order wins.
		[[nodiscard]] Direction BufferedDirection() const noexcept { return bufferedDirection; }
		void ClearBufferedDirection() noexcept { bufferedDirection = Direction::None; }
	};
} // namespace game
