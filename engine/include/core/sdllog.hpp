#pragma once

// Part of the platform library (engine::platform), not engine::core: it needs SDL.
namespace engine::core
{
	// Sends SDL's own log (SDL_Log, SDL_LogError, ...) through core::Log, so the engine's and SDL's
	// messages land in the same file. Call it once, right after making the LogSession; it can be
	// called again harmlessly. It stays installed for the rest of the run. While no LogSession
	// exists, SDL's messages go to its default output as before. Application calls it too, so a
	// game that forgets still gets routed once the window code starts.
	void RouteSdlLog();
} // namespace engine::core
