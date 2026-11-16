#pragma once
#include <filesystem>

namespace engine::core
{
	// The folder that holds the running executable. The build copies the game's assets next to
	// it, so the game finds them there wherever it was started from, unlike paths relative to
	// the working directory. Empty if the platform can't say; the reason is logged.
	[[nodiscard]] std::filesystem::path ExecutableDirectory();

	// The folder this user's saved data belongs in, made if it doesn't exist yet: on Windows
	// %APPDATA%\organisation\application, on Linux ~/.local/share/application, on macOS
	// ~/Library/Application Support/application. Empty if the platform can't say; the reason
	// is logged.
	[[nodiscard]] std::filesystem::path SaveDirectory(const char* organisation, const char* application);
} // namespace engine::core
