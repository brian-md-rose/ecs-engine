#pragma once

namespace game
{
	// Game-wide flags, kept as a resource. While stressTest is on, an engine::core::FrameStats
	// resource prints how fast the game draws.
	struct GameState
	{
		bool paused = false;
		bool stressTest = false;
		bool quitRequested = false; // the title screen's Quit: the application closes
	};
} // namespace game
