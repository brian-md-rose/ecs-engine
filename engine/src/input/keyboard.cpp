#include "input/keyboard.hpp"

#include "core/assert.hpp"

namespace engine::input
{
	namespace
	{
		std::size_t Index(Key key)
		{
			const auto index = static_cast<std::size_t>(key);
			ENGINE_ASSERT(index < kKeyCount, "invalid key");
			return index;
		}
	} // namespace

	void Keyboard::OnKeyDown(Key key)
	{
		const std::size_t k = Index(key);
		if (down[k]) return; // OS key repeat
		down[k] = true;
		pressed[k] = true;
	}

	void Keyboard::OnKeyUp(Key key)
	{
		const std::size_t k = Index(key);
		if (!down[k]) return;
		down[k] = false;
		released[k] = true;
	}

	void Keyboard::ReleaseAll()
	{
		for (std::size_t k = 0; k < kKeyCount; ++k)
		{
			OnKeyUp(static_cast<Key>(k));
		}
	}

	void Keyboard::EndUpdate()
	{
		pressed.fill(false);
		released.fill(false);
	}

	bool Keyboard::IsDown(Key key) const
	{
		return down[Index(key)];
	}

	bool Keyboard::WasPressed(Key key) const
	{
		return pressed[Index(key)];
	}

	bool Keyboard::WasReleased(Key key) const
	{
		return released[Index(key)];
	}
} // namespace engine::input
