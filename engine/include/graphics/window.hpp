#pragma once

// SDL types are only forward-declared here: SDL is linked PRIVATE to the engine, so its headers
// are not visible to the game or tests. Only engine .cpp files include <SDL3/...>.
#include <vector>

#include "graphics/display.hpp"

struct SDL_Window;

namespace engine::graphics
{
	// Owns the OS window and SDL's video subsystem.
	class Window
	{
		SDL_Window* window = nullptr;
		bool videoInitialized = false;

	public:
		Window(const char* title, int width, int height);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;
		Window(Window&&) = delete;
		Window& operator=(Window&&) = delete;

		[[nodiscard]] bool IsValid() const noexcept { return window != nullptr; }

		// For engine code (e.g. Renderer) that needs the SDL handle. SDL_Window is only
		// forward-declared, so this doesn't expose SDL's headers.
		[[nodiscard]] SDL_Window* GetWindowHandle() const noexcept { return window; }

		// Puts the window in mode. Windowed: a framed window of width x height (it leaves fullscreen first).
		// Borderless: the desktop's size, no frame, no display change; width and height are ignored.
		// Fullscreen: the display switches to the closest mode to width x height. Returns false, with the reason
		// logged, if SDL refused (the window is then left as it was, or as far as SDL got).
		bool SetMode(WindowMode mode, int width, int height);

		// The sizes the window's display can be switched to for Fullscreen, largest first, each once. Empty if
		// the window is invalid or the display lists none.
		[[nodiscard]] std::vector<Resolution> Resolutions() const;
	};
} // namespace engine::graphics
