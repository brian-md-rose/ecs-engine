#include "input/input.hpp"

#include <algorithm>
#include <cmath>

namespace engine::input
{
	void Input::ReleaseAll()
	{
		keyboard.ReleaseAll();
		mouse.ReleaseAll();
		for (Gamepad& pad : pads)
		{
			pad.ReleaseAll();
		}
		typed.clear();
	}

	void Input::EndUpdate()
	{
		keyboard.EndUpdate();
		mouse.EndUpdate();
		for (Gamepad& pad : pads)
		{
			pad.EndUpdate();
		}
		typed.clear();
	}

	bool Input::PadDown(PadButton button) const
	{
		return std::ranges::any_of(pads, [button](const Gamepad& pad)
		{
			return pad.connected && pad.IsDown(button);
		});
	}

	bool Input::PadPressed(PadButton button) const
	{
		return std::ranges::any_of(pads, [button](const Gamepad& pad)
		{
			return pad.connected && pad.WasPressed(button);
		});
	}

	bool Input::PadReleased(PadButton button) const
	{
		return std::ranges::any_of(pads, [button](const Gamepad& pad)
		{
			return pad.connected && pad.WasReleased(button);
		});
	}

	bool Input::PadPushed(PadDirection direction) const
	{
		return std::ranges::any_of(pads, [direction](const Gamepad& pad)
		{
			return pad.connected && pad.DirectionWasPushed(direction);
		});
	}

	bool Input::PadDirectionHeld(PadDirection direction) const
	{
		return std::ranges::any_of(pads, [direction](const Gamepad& pad)
		{
			return pad.connected && pad.DirectionHeld(direction);
		});
	}

	Stick Input::PadRightStick() const
	{
		float x = 0.0f;
		float y = 0.0f;
		for (const Gamepad& pad : pads)
		{
			if (!pad.connected) continue;
			x += pad.Right().x;
			y += pad.Right().y;
		}
		const float length = std::hypot(x, y);
		if (length > 1.0f)
		{
			x /= length;
			y /= length;
		}
		return {.x = x, .y = y};
	}

	PadFamily Input::PadFamilyInUse() const
	{
		for (const Gamepad& pad : pads)
		{
			if (pad.connected) return pad.family;
		}
		return PadFamily::Generic;
	}
} // namespace engine::input
