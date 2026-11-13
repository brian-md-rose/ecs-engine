#pragma once

// SDL types are only forward-declared here: SDL is linked PRIVATE to the engine, so its headers
// are not visible to the game or tests. Only engine .cpp files include <SDL3/...>.
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

		// Drains pending window events. Returns false once the window has been asked to close.
		// Stopgap until the Input module (#9) owns event handling.
		bool PollEvents();

		// For engine code (e.g. Renderer) that needs the SDL handle. SDL_Window is only
		// forward-declared, so this doesn't expose SDL's headers.
		[[nodiscard]] SDL_Window* GetWindowHandle() const noexcept { return window; }
	};
} // namespace engine::graphics
