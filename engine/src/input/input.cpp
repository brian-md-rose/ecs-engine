#include "input/input.hpp"

namespace engine::input
{
	void Input::ReleaseAll()
	{
		keyboard.ReleaseAll();
		typed.clear();
	}

	void Input::EndUpdate()
	{
		keyboard.EndUpdate();
		typed.clear();
	}
} // namespace engine::input
