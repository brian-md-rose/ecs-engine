#include "core/paths.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <string>

namespace engine::core
{
	std::filesystem::path ExecutableDirectory()
	{
		// Owned and cached by SDL; works without SDL_Init.
		const char* base = SDL_GetBasePath();
		if (base == nullptr)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetBasePath failed: %s", SDL_GetError());
			return {};
		}
		// SDL returns UTF-8. Say so, or Windows would read it in the local code page.
		return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(base)));
	}

	std::filesystem::path SaveDirectory(const char* organisation, const char* application)
	{
		// Made by SDL, and ours to free; works without SDL_Init.
		char* folder = SDL_GetPrefPath(organisation, application);
		if (folder == nullptr)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetPrefPath failed: %s", SDL_GetError());
			return {};
		}
		std::filesystem::path path(std::u8string(reinterpret_cast<const char8_t*>(folder)));
		SDL_free(folder);
		return path;
	}
} // namespace engine::core
