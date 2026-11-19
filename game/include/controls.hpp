#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "grid/direction.hpp"
#include "input/gamepad.hpp"
#include "input/input.hpp"
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
		ToggleStressTest, // debug: fill the screen with shapes to check rendering speed
		Count
	};

	inline constexpr std::size_t kActionCount = static_cast<std::size_t>(Action::Count);

	// Maps engine keys to game actions. Call Update() once at the start of each game update,
	// then query actions for the rest of it.
	class Controls
	{
		std::array<std::optional<Action>, engine::input::kKeyCount> bindings{};
		std::array<std::optional<Action>, engine::input::kPadButtonCount> padBindings{};
		std::array<bool, kActionCount> held{};
		std::array<bool, kActionCount> pressed{};
		std::array<bool, kActionCount> released{};
		engine::grid::Direction bufferedDirection = engine::grid::Direction::None;

		void Update(const engine::input::Keyboard& keyboard, const engine::input::Input* input);

	public:
		// Starts with the default bindings: arrows and WASD to move, P or Escape to pause,
		// Enter or Space to confirm. On a pad: the d-pad or left stick to move, South to confirm,
		// Start or Back to pause.
		Controls();

		// Binds key to action, replacing any previous binding of that key. Safe while keys are
		// held: action state is recomputed from the keyboard on the next Update().
		void Bind(engine::input::Key key, Action action);
		void Unbind(engine::input::Key key);
		[[nodiscard]] std::optional<Action> GetBinding(engine::input::Key key) const;

		// Binds a pad button to an action, on any connected pad. The four directions are not bound this way: the d-pad
		// and the left stick always move.
		void BindPad(engine::input::PadButton button, Action action);
		void UnbindPad(engine::input::PadButton button);
		[[nodiscard]] std::optional<Action> GetPadBinding(engine::input::PadButton button) const;

		void Update(const engine::input::Keyboard& keyboard);
		// The keyboard, and the pads in the input too.
		void Update(const engine::input::Input& input);

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
		[[nodiscard]] engine::grid::Direction BufferedDirection() const noexcept { return bufferedDirection; }
		void ClearBufferedDirection() noexcept { bufferedDirection = engine::grid::Direction::None; }
	};
} // namespace game
