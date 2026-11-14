#pragma once
#include <filesystem>

namespace engine::core
{
	// The folder that holds the running executable. The build copies the game's assets next to
	// it, so the game finds them there wherever it was started from, unlike paths relative to
	// the working directory. Empty if the platform can't say; the reason is logged.
	[[nodiscard]] std::filesystem::path ExecutableDirectory();
} // namespace engine::core
