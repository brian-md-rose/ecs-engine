#include "input/input.hpp"

namespace engine::input
{
	void Input::ReleaseAll()
	{
		keyboard.ReleaseAll();
		mouse.ReleaseAll();
		typed.clear();
	}

	void Input::EndUpdate()
	{
		keyboard.EndUpdate();
		mouse.EndUpdate();
		typed.clear();
	}
} // namespace engine::input
