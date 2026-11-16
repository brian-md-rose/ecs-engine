#pragma once

namespace game
{
	// Game-wide flags, kept as a resource. stressTest fills the screen with shapes; the debug
	// panel (F3) shows how fast they draw.
	struct GameState
	{
		bool paused = false;
		bool stressTest = false;
		bool quitRequested = false; // the title screen's Quit: the application closes
	};
} // namespace game
