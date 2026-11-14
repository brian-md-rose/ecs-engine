#include "core/paths.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <string>

namespace engine::core
{
	std::filesystem::path ExecutableDirectory()
	{
		// Owned and cached by SDL; works without SDL_Init.
		const char* base = SDL_GetBasePath();
		if (base == nullptr)
		{
			SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
			return {};
		}
		// SDL returns UTF-8. Say so, or Windows would read it in the local code page.
		return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(base)));
	}
} // namespace engine::core
