#include "graphics/imagefile.hpp"

#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "core/text.hpp"

namespace engine::graphics
{
	namespace
	{
		struct SurfaceDeleter
		{
			void operator()(SDL_Surface* surface) const noexcept { SDL_DestroySurface(surface); }
		};
		using SurfacePtr = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

		[[nodiscard]] ImageResult Fail(std::string message)
		{
			return {.image = std::nullopt, .error = std::move(message)};
		}
	} // namespace

	ImageResult LoadImage(const std::filesystem::path& file)
	{
		const std::string path = core::PathText(file);
		SurfacePtr loaded(SDL_LoadPNG(path.c_str()));
		if (!loaded) return Fail(std::string("can't load the PNG: ") + SDL_GetError());

		SurfacePtr rgba(SDL_ConvertSurface(loaded.get(), SDL_PIXELFORMAT_RGBA32));
		if (!rgba) return Fail(std::string("can't convert the PNG to RGBA: ") + SDL_GetError());

		Image image(rgba->w, rgba->h);
		const auto* bytes = static_cast<const std::uint8_t*>(rgba->pixels);
		for (int y = 0; y < rgba->h; ++y)
		{
			const std::uint8_t* row = bytes + static_cast<std::ptrdiff_t>(y) * rgba->pitch;
			for (int x = 0; x < rgba->w; ++x)
			{
				const std::uint8_t* p = row + static_cast<std::ptrdiff_t>(x) * 4;
				image.Set(x, y, Rgba{p[0], p[1], p[2], p[3]});
			}
		}
		return {.image = std::move(image), .error = {}};
	}

	bool SaveImage(const Image& image, const std::filesystem::path& file)
	{
		if (image.IsEmpty()) return false;
		const std::string path = core::PathText(file);
		// The surface only borrows the pixels, and SDL_SavePNG doesn't write to them.
		void* pixels = const_cast<Rgba*>(image.Pixels().data());
		SurfacePtr surface(
		    SDL_CreateSurfaceFrom(image.Width(), image.Height(), SDL_PIXELFORMAT_RGBA32, pixels, image.Width() * 4));
		if (!surface || !SDL_SavePNG(surface.get(), path.c_str()))
		{
			SDL_Log("SaveImage failed: %s", SDL_GetError());
			return false;
		}
		return true;
	}
} // namespace engine::graphics
