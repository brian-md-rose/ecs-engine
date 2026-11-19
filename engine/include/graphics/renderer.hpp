#pragma once
#include <cstdint>
#include <vector>

#include "graphics/display.hpp"

// SDL types are only forward-declared here: SDL is linked PRIVATE to the engine, so its headers
// are not visible to the game or tests. Only engine .cpp files include <SDL3/...>.
struct SDL_Renderer;
struct SDL_Texture;

namespace engine::graphics
{
	class Image;
	class Window;

	// Names a texture the Renderer owns. 0 (kNoTexture) is never a texture.
	using TextureId = std::uint32_t;
	inline constexpr TextureId kNoTexture = 0;

	// Owns the SDL renderer, and every texture made with it, for the lifetime of the object.
	// Draws into a Window, which must outlive the Renderer (declare the Window first).
	class Renderer
	{
		SDL_Renderer* renderer = nullptr;
		std::vector<SDL_Texture*> textures; // TextureId i is textures[i - 1]; null once destroyed
		int logicalWidth = 0;
		int logicalHeight = 0;
		Scaling scaling = Scaling::Integer;

	public:
		// renderWidth/renderHeight is the game's fixed logical resolution. With Scaling::Integer (the default) it
		// is scaled up by the largest whole number that fits the window, with black bars around the rest; with
		// Scaling::Fit by any amount, letterboxed. VSync starts on.
		Renderer(Window& window, int renderWidth, int renderHeight, Scaling scaling = Scaling::Integer);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer(Renderer&&) = delete;
		Renderer& operator=(Renderer&&) = delete;

		// False if the renderer couldn't be created (the reason is logged).
		[[nodiscard]] bool IsValid() const noexcept { return renderer != nullptr; }

		// Changes how the logical picture is scaled to the window, from the next frame. Returns false (the reason
		// logged) if SDL refused.
		bool SetScaling(Scaling mode);
		[[nodiscard]] Scaling GetScaling() const noexcept { return scaling; }

		// VSync on or off. Returns false (logged) if SDL refused, as some drivers do for off.
		bool SetVSync(bool enabled);

		void Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b);

		// Uploads image as a texture with alpha blending. It is sampled nearest-neighbour, so
		// its pixels stay sharp when the logical resolution is scaled up to the window. Returns
		// kNoTexture if it couldn't be created (the reason is logged). The texture lives until
		// DestroyTexture or the Renderer's end, whichever comes first.
		[[nodiscard]] TextureId CreateTexture(const Image& image);
		void DestroyTexture(TextureId texture);

		// A texture that can be drawn into, width by height pixels, for drawing a picture off screen (the editor's
		// viewport). Draw into it between SetRenderTarget(it) and SetRenderTarget(kNoTexture); then it is drawn like
		// any texture. While it is the target, positions are in its pixels, not the window's logical size. Returns
		// kNoTexture if it couldn't be made (logged). Destroyed like any texture.
		[[nodiscard]] TextureId CreateRenderTarget(int width, int height);
		// Draws into texture from now on, or into the window again for kNoTexture. False (logged) if SDL refused or the
		// texture is not a render target.
		bool SetRenderTarget(TextureId texture);

		// For engine code that draws with SDL directly, such as Renderer2D.
		[[nodiscard]] SDL_Renderer* GetRendererHandle() const noexcept { return renderer; }
		// nullptr for kNoTexture or a texture that no longer exists.
		[[nodiscard]] SDL_Texture* GetTextureHandle(TextureId texture) const noexcept;

		void Present();
	};
} // namespace engine::graphics
