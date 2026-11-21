#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "graphics/image.hpp"

// The parts of screenshots that need no window: the picture's size, the file's name and the folder it goes in.
// Application does the capture itself (F12, or a game's ScreenshotRequest); see docs/screenshots.md.
namespace engine::graphics
{
	// Resource: a game sets pending to ask for a screenshot of the next frame. Application clears it after the
	// frame, whether or not the file was written (see ScreenshotResult).
	struct ScreenshotRequest
	{
		bool pending = false;
	};

	// Resource, written by Application: what the last screenshot did.
	struct ScreenshotResult
	{
		std::filesystem::path lastFile; // the last file written, empty if none yet
		std::string lastError;          // why the last attempt failed, empty if it didn't
		std::uint64_t count = 0;        // files written so far
	};

	// A local date and time, for naming a file.
	struct ScreenshotTime
	{
		int year = 1970;
		int month = 1;
		int day = 1;
		int hour = 0;
		int minute = 0;
		int second = 0;
	};

	// "screenshot-20261121-104530.png", or "screenshot-20261121-104530-2.png" for suffix 2 (suffix 0 and 1 add
	// nothing). Fields are clamped to their range so the name always has the same shape.
	[[nodiscard]] std::string ScreenshotFileName(const ScreenshotTime& time, int suffix = 0);

	// folder / ScreenshotFileName(time), with the first suffix (2, 3, ...) that doesn't name an existing file, so
	// two screenshots in one second never overwrite each other.
	[[nodiscard]] std::filesystem::path UniqueScreenshotPath(const std::filesystem::path& folder,
	                                                         const ScreenshotTime& time);

	// Where screenshots go. In a development build, the repository's screenshot folder (its path is set when
	// the engine is configured, see ENGINE_SCREENSHOT_DIR in CMake) when that folder exists. Otherwise
	// saveDirectory / "screenshots", next to the log, or empty if saveDirectory is empty.
	[[nodiscard]] std::filesystem::path ScreenshotDirectory(const std::filesystem::path& saveDirectory);

	// As above with the repository folder given, so the choice can be tested whatever this build was configured with.
	[[nodiscard]] std::filesystem::path ScreenshotDirectory(const std::filesystem::path& saveDirectory,
	                                                        const std::filesystem::path& repositoryDirectory);

	// image scaled to width x height by averaging the source pixels each new pixel covers, alpha included.
	// Shrinking by a whole number (a 4x integer-scaled picture back to its logical size) gives back the original
	// pixels exactly. An empty image, or a non-positive size, gives an empty image.
	[[nodiscard]] Image ResizeImage(const Image& image, int width, int height);

	// The part of image inside rect, which is clipped to the image. Empty if nothing is left.
	[[nodiscard]] Image CropImage(const Image& image, PixelRect rect);
} // namespace engine::graphics
