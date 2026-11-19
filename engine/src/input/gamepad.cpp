#include "input/gamepad.hpp"

#include <algorithm>
#include <cmath>

#include "core/assert.hpp"

namespace engine::input
{
	namespace
	{
		std::size_t Index(PadButton button)
		{
			const auto index = static_cast<std::size_t>(button);
			ENGINE_ASSERT(index < kPadButtonCount, "invalid pad button");
			return index;
		}

		PadButton DPadButton(PadDirection direction) noexcept
		{
			switch (direction)
			{
			case PadDirection::Up:
				return PadButton::DUp;
			case PadDirection::Down:
				return PadButton::DDown;
			case PadDirection::Left:
				return PadButton::DLeft;
			case PadDirection::Right:
				return PadButton::DRight;
			}
			return PadButton::DUp;
		}

		bool Past(Stick stick, PadDirection direction, float threshold) noexcept
		{
			switch (direction)
			{
			case PadDirection::Up:
				return stick.y <= -threshold;
			case PadDirection::Down:
				return stick.y >= threshold;
			case PadDirection::Left:
				return stick.x <= -threshold;
			case PadDirection::Right:
				return stick.x >= threshold;
			}
			return false;
		}

		bool Contains(std::string_view text, std::string_view part) noexcept
		{
			return text.find(part) != std::string_view::npos;
		}
	} // namespace

	Stick ApplyDeadzone(float x, float y, float deadzone) noexcept
	{
		const float magnitude = std::hypot(x, y);
		if (magnitude <= deadzone || magnitude <= 0.0f) return {};
		const float limit = std::clamp(deadzone, 0.0f, 0.99f);
		const float scaled = std::min((magnitude - limit) / (1.0f - limit), 1.0f);
		return {.x = x / magnitude * scaled, .y = y / magnitude * scaled};
	}

	float ApplyTriggerDeadzone(float value, float deadzone) noexcept
	{
		if (value <= deadzone) return 0.0f;
		const float limit = std::clamp(deadzone, 0.0f, 0.99f);
		return std::min((value - limit) / (1.0f - limit), 1.0f);
	}

	std::string_view PadButtonLabel(PadFamily family, PadButton button) noexcept
	{
		// Positions in the order South, East, West, North, Start, Back, L1, R1, LStick, RStick, then the d-pad.
		struct Labels
		{
			std::array<std::string_view, kPadButtonCount> names;
		};
		static constexpr Labels kXbox{
		    {"A", "B", "X", "Y", "MENU", "VIEW", "LB", "RB", "LS", "RS", "UP", "DOWN", "LEFT", "RIGHT"}};
		static constexpr Labels kPlayStation{{"CROSS", "CIRCLE", "SQUARE", "TRIANGLE", "OPTIONS", "SHARE", "L1", "R1",
		                                      "L3", "R3", "UP", "DOWN", "LEFT", "RIGHT"}};
		// A Switch pad's A is on the right (East) and its B at the bottom (South).
		static constexpr Labels kSwitch{
		    {"B", "A", "Y", "X", "+", "-", "L", "R", "LS", "RS", "UP", "DOWN", "LEFT", "RIGHT"}};
		static constexpr Labels kSteamDeck{
		    {"A", "B", "X", "Y", "MENU", "VIEW", "L1", "R1", "L3", "R3", "UP", "DOWN", "LEFT", "RIGHT"}};
		static constexpr Labels kGeneric{
		    {"SOUTH", "EAST", "WEST", "NORTH", "START", "BACK", "L1", "R1", "L3", "R3", "UP", "DOWN", "LEFT", "RIGHT"}};

		const auto index = static_cast<std::size_t>(button);
		if (index >= kPadButtonCount) return {};
		switch (family)
		{
		case PadFamily::Xbox:
			return kXbox.names[index];
		case PadFamily::PlayStation:
			return kPlayStation.names[index];
		case PadFamily::Switch:
			return kSwitch.names[index];
		case PadFamily::SteamDeck:
			return kSteamDeck.names[index];
		case PadFamily::Generic:
			break;
		}
		return kGeneric.names[index];
	}

	bool IsSteamDeck(std::string_view name, std::uint16_t vendor, std::uint16_t product) noexcept
	{
		constexpr std::uint16_t kValve = 0x28de;
		constexpr std::uint16_t kDeckController = 0x1205;
		return (vendor == kValve && product == kDeckController) || Contains(name, "Steam Deck");
	}

	void Gamepad::Connect(PadFamily padFamily)
	{
		// Held buttons are let go with a release edge (seen until the next EndUpdate), so the game isn't left holding.
		ReleaseAll();
		leftPushed.fill(false);
		connected = false;
		family = PadFamily::Generic;
		connected = true;
		family = padFamily;
	}

	void Gamepad::Disconnect()
	{
		// Held buttons are let go with a release edge (seen until the next EndUpdate), so the game isn't left holding.
		ReleaseAll();
		leftPushed.fill(false);
		connected = false;
		family = PadFamily::Generic;
	}

	void Gamepad::OnButton(PadButton button, bool isDown)
	{
		const std::size_t i = Index(button);
		if (isDown == down[i]) return;
		down[i] = isDown;
		(isDown ? pressed : released)[i] = true;
	}

	bool Gamepad::OnAxis(PadAxis axis, float value)
	{
		switch (axis)
		{
		case PadAxis::LeftX:
		case PadAxis::LeftY:
			(axis == PadAxis::LeftX ? rawLeft.x : rawLeft.y) = value;
			left = ApplyDeadzone(rawLeft.x, rawLeft.y, deadzone);
			LatchPushes();
			return left != Stick{};
		case PadAxis::RightX:
		case PadAxis::RightY:
			(axis == PadAxis::RightX ? rawRight.x : rawRight.y) = value;
			right = ApplyDeadzone(rawRight.x, rawRight.y, deadzone);
			return right != Stick{};
		case PadAxis::LeftTrigger:
			leftTrigger = ApplyTriggerDeadzone(value, deadzone);
			return leftTrigger > 0.0f;
		case PadAxis::RightTrigger:
			rightTrigger = ApplyTriggerDeadzone(value, deadzone);
			return rightTrigger > 0.0f;
		}
		return false;
	}

	void Gamepad::ReleaseAll()
	{
		for (std::size_t i = 0; i < kPadButtonCount; ++i)
		{
			if (down[i]) released[i] = true;
		}
		down.fill(false);
		rawLeft = rawRight = left = right = Stick{};
		leftPast.fill(false);
		leftTrigger = rightTrigger = 0.0f;
	}

	void Gamepad::EndUpdate()
	{
		pressed.fill(false);
		released.fill(false);
		leftPushed.fill(false);
	}

	bool Gamepad::IsDown(PadButton button) const
	{
		return down[Index(button)];
	}

	bool Gamepad::WasPressed(PadButton button) const
	{
		return pressed[Index(button)];
	}

	bool Gamepad::WasReleased(PadButton button) const
	{
		return released[Index(button)];
	}

	bool Gamepad::StickHeld(PadDirection direction, float threshold) const noexcept
	{
		return Past(left, direction, threshold);
	}

	void Gamepad::LatchPushes() noexcept
	{
		for (std::size_t d = 0; d < leftPast.size(); ++d)
		{
			const bool past = Past(left, static_cast<PadDirection>(d), kPushThreshold);
			if (past && !leftPast[d]) leftPushed[d] = true;
			leftPast[d] = past;
		}
	}

	bool Gamepad::StickWasPushed(PadDirection direction) const noexcept
	{
		return leftPushed[static_cast<std::size_t>(direction)];
	}

	bool Gamepad::DirectionHeld(PadDirection direction) const
	{
		return IsDown(DPadButton(direction)) || StickHeld(direction);
	}

	bool Gamepad::DirectionWasPushed(PadDirection direction) const
	{
		return WasPressed(DPadButton(direction)) || StickWasPushed(direction);
	}
} // namespace engine::input
