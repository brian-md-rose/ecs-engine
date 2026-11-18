#include "input/mouse.hpp"

#include "core/assert.hpp"

namespace engine::input
{
	namespace
	{
		std::size_t Index(MouseButton button)
		{
			const auto index = static_cast<std::size_t>(button);
			ENGINE_ASSERT(index < kMouseButtonCount, "invalid mouse button");
			return index;
		}
	} // namespace

	void Mouse::OnMove(graphics::Position where, bool isInside)
	{
		position = where;
		inside = isInside;
	}

	void Mouse::OnButton(MouseButton button, bool isDown)
	{
		const std::size_t b = Index(button);
		if (down[b] == isDown) return;
		down[b] = isDown;
		(isDown ? pressed : released)[b] = true;
	}

	void Mouse::ReleaseAll()
	{
		for (std::size_t b = 0; b < kMouseButtonCount; ++b)
		{
			OnButton(static_cast<MouseButton>(b), false);
		}
	}

	void Mouse::EndUpdate()
	{
		pressed.fill(false);
		released.fill(false);
		wheel = 0;
	}

	bool Mouse::IsDown(MouseButton button) const
	{
		return down[Index(button)];
	}

	bool Mouse::WasPressed(MouseButton button) const
	{
		return pressed[Index(button)];
	}

	bool Mouse::WasReleased(MouseButton button) const
	{
		return released[Index(button)];
	}
} // namespace engine::input
