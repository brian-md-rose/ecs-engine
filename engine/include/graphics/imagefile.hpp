#pragma once
#include <filesystem>
#include <optional>
#include <string>

#include "graphics/image.hpp"

// Reads and writes PNG files as Images. Needs no SDL_Init and no window.
namespace engine::graphics
{
	// Either an image, or why the file couldn't be loaded.
	struct ImageResult
	{
		std::optional<Image> image;
		std::string error;
	};

	// The PNG at file, as straight-alpha RGBA whatever its own pixel format.
	[[nodiscard]] ImageResult LoadImage(const std::filesystem::path& file);

	// Writes image as a PNG; false if it is empty or can't be written (the reason is logged).
	[[nodiscard]] bool SaveImage(const Image& image, const std::filesystem::path& file);
} // namespace engine::graphics
