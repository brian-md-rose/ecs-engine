#pragma once
#include <cstdint>

// SDL types are only forward-declared here: SDL is linked PRIVATE to the engine, so its headers
// are not visible to the game or tests. Only engine .cpp files include <SDL3/...>.
struct SDL_Renderer;

namespace engine::graphics
{
	class Window;

	// Owns the SDL renderer for the lifetime of the object. Draws into a Window,
	// which must outlive the Renderer (declare the Window first).
	class Renderer
	{
		SDL_Renderer* renderer = nullptr;

	public:
		// renderWidth/renderHeight is the game's fixed logical resolution. It is scaled up by
		// the largest whole number that fits the window, with black bars around the rest.
		Renderer(Window& window, int renderWidth, int renderHeight);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer(Renderer&&) = delete;
		Renderer& operator=(Renderer&&) = delete;

		// False if the renderer couldn't be created (the reason is logged).
		[[nodiscard]] bool IsValid() const noexcept { return renderer != nullptr; }

		void Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b);

		// For engine code that draws with SDL directly, such as Renderer2D.
		[[nodiscard]] SDL_Renderer* GetRendererHandle() const noexcept { return renderer; }

		void Present();
	};
} // namespace engine::graphics
