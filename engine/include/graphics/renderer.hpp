#pragma once
#include <cstdint>

// SDL types are only forward-declared here: SDL is linked PRIVATE to the engine, so its headers
// are not visible to the game or tests. Only engine .cpp files include <SDL3/...>.
struct SDL_Window;
struct SDL_Renderer;

namespace engine::graphics
{
	// Owns the SDL window and renderer for the lifetime of the object.
	class Renderer
	{
		SDL_Window* window = nullptr;
		SDL_Renderer* renderer = nullptr;

	public:
		Renderer(const char* title, int width, int height);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer(Renderer&&) = delete;
		Renderer& operator=(Renderer&&) = delete;

		// False if the window or renderer couldn't be created (the reason is logged).
		[[nodiscard]] bool IsValid() const noexcept { return window != nullptr && renderer != nullptr; }

		// Drains pending window events. Returns false once the window has been asked to close.
		// Stopgap until the Input module (#9) owns event handling.
		bool PollEvents();

		void Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b);
		void Present();
	};
} // namespace engine::graphics
