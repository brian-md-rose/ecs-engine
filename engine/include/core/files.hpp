#pragma once

#include <filesystem>
#include <string_view>

// File helpers shared by the engine's writers (save data, JSON). No SDL.
namespace engine::core
{
	// Writes contents to file, making its folder if needed. It writes a temporary file beside it
	// first and then replaces file with it, so a crash of the program part-way leaves the old file
	// intact. It does not force the data to disk, so a power cut can still lose the write. False
	// if anything failed; the reason is logged.
	bool WriteFileAtomic(const std::filesystem::path& file, std::string_view contents);
} // namespace engine::core
