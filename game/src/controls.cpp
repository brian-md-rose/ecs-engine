#include "controls.hpp"

#include <utility>

#include "core/assert.hpp"

namespace game
{
	using engine::grid::Direction;

	namespace
	{
		using engine::input::Key;
		using engine::input::kKeyCount;

		std::size_t Index(Key key)
		{
			const auto index = static_cast<std::size_t>(key);
			ENGINE_ASSERT(index < kKeyCount, "invalid key");
			return index;
		}

		std::size_t Index(Action action)
		{
			const auto index = static_cast<std::size_t>(action);
			ENGINE_ASSERT(index < kActionCount, "invalid action");
			return index;
		}

		Direction ToDirection(Action action)
		{
			if (action == Action::MoveUp) return Direction::Up;
			if (action == Action::MoveDown) return Direction::Down;
			if (action == Action::MoveLeft) return Direction::Left;
			if (action == Action::MoveRight) return Direction::Right;
			return Direction::None;
		}
	} // namespace

	Controls::Controls()
	{
		Bind(Key::Up, Action::MoveUp);
		Bind(Key::Down, Action::MoveDown);
		Bind(Key::Left, Action::MoveLeft);
		Bind(Key::Right, Action::MoveRight);
		Bind(Key::W, Action::MoveUp);
		Bind(Key::S, Action::MoveDown);
		Bind(Key::A, Action::MoveLeft);
		Bind(Key::D, Action::MoveRight);
		Bind(Key::P, Action::Pause);
		Bind(Key::Escape, Action::Pause);
		Bind(Key::Enter, Action::Confirm);
		Bind(Key::Space, Action::Confirm);
		Bind(Key::F1, Action::ToggleStressTest);
		BindPad(engine::input::PadButton::South, Action::Confirm);
		BindPad(engine::input::PadButton::Start, Action::Pause);
		BindPad(engine::input::PadButton::Back, Action::Pause);
	}

	void Controls::BindPad(engine::input::PadButton button, Action action)
	{
		ENGINE_ASSERT(static_cast<std::size_t>(action) < kActionCount, "invalid action");
		ENGINE_ASSERT(static_cast<std::size_t>(button) < engine::input::kPadButtonCount, "invalid pad button");
		padBindings[static_cast<std::size_t>(button)] = action;
	}

	void Controls::UnbindPad(engine::input::PadButton button)
	{
		ENGINE_ASSERT(static_cast<std::size_t>(button) < engine::input::kPadButtonCount, "invalid pad button");
		padBindings[static_cast<std::size_t>(button)].reset();
	}

	std::optional<Action> Controls::GetPadBinding(engine::input::PadButton button) const
	{
		ENGINE_ASSERT(static_cast<std::size_t>(button) < engine::input::kPadButtonCount, "invalid pad button");
		return padBindings[static_cast<std::size_t>(button)];
	}

	void Controls::Bind(Key key, Action action)
	{
		ENGINE_ASSERT(static_cast<std::size_t>(action) < kActionCount, "invalid action");
		bindings[Index(key)] = action;
	}

	void Controls::Unbind(Key key)
	{
		bindings[Index(key)].reset();
	}

	std::optional<Action> Controls::GetBinding(Key key) const
	{
		return bindings[Index(key)];
	}

	void Controls::Update(const engine::input::Keyboard& keyboard)
	{
		Update(keyboard, nullptr);
	}

	void Controls::Update(const engine::input::Input& input)
	{
		Update(input.keyboard, &input);
	}

	void Controls::Update(const engine::input::Keyboard& keyboard, const engine::input::Input* input)
	{
		std::array<bool, kActionCount> anyDown{};
		std::array<bool, kActionCount> anyPressed{};
		for (std::size_t k = 0; k < kKeyCount; ++k)
		{
			if (!bindings[k]) continue;
			const Key key = static_cast<Key>(k);
			const std::size_t a = Index(*bindings[k]);
			anyDown[a] = anyDown[a] || keyboard.IsDown(key);
			anyPressed[a] = anyPressed[a] || keyboard.WasPressed(key);
		}

		if (input != nullptr)
		{
			using engine::input::PadDirection;
			for (std::size_t b = 0; b < engine::input::kPadButtonCount; ++b)
			{
				if (!padBindings[b]) continue;
				const auto button = static_cast<engine::input::PadButton>(b);
				const std::size_t a = Index(*padBindings[b]);
				anyDown[a] = anyDown[a] || input->PadDown(button);
				anyPressed[a] = anyPressed[a] || input->PadPressed(button);
			}
			// The d-pad and the left stick are the same thing to the game: a direction.
			const std::array<std::pair<PadDirection, Action>, 4> directions = {
			    {{PadDirection::Up, Action::MoveUp},
			     {PadDirection::Down, Action::MoveDown},
			     {PadDirection::Left, Action::MoveLeft},
			     {PadDirection::Right, Action::MoveRight}}};
			for (const auto& [direction, action] : directions)
			{
				const std::size_t a = Index(action);
				anyDown[a] = anyDown[a] || input->PadDirectionHeld(direction);
				anyPressed[a] = anyPressed[a] || input->PadPushed(direction);
			}
		}

		for (std::size_t a = 0; a < kActionCount; ++a)
		{
			const bool wasHeld = held[a];
			held[a] = anyDown[a];
			// anyPressed rather than held && !wasHeld, so a tap that went down and up between
			// two updates still counts as a press.
			pressed[a] = anyPressed[a] && !wasHeld;
			released[a] = (wasHeld || pressed[a]) && !held[a];

			// Every new press of a direction key counts, even if the action is already held.
			if (anyPressed[a])
			{
				if (const Direction direction = ToDirection(static_cast<Action>(a)); direction != Direction::None)
				{
					bufferedDirection = direction;
				}
			}
		}
	}

	bool Controls::IsHeld(Action action) const
	{
		return held[Index(action)];
	}

	bool Controls::WasPressed(Action action) const
	{
		return pressed[Index(action)];
	}

	bool Controls::WasReleased(Action action) const
	{
		return released[Index(action)];
	}
} // namespace game
