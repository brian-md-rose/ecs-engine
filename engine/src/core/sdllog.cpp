#include "core/sdllog.hpp"

#include <SDL3/SDL_log.h>

#include "core/log.hpp"

namespace engine::core
{
	namespace
	{
		// An if chain, not a switch: SDL_LogPriority has enumerators we have no use for, and /Wall warns about
		// every one a switch leaves unhandled.
		LogLevel FromSdl(SDL_LogPriority priority)
		{
			if (priority == SDL_LOG_PRIORITY_WARN) return LogLevel::Warning;
			if (priority == SDL_LOG_PRIORITY_ERROR || priority == SDL_LOG_PRIORITY_CRITICAL) return LogLevel::Error;
			if (priority == SDL_LOG_PRIORITY_INFO) return LogLevel::Info;
			return LogLevel::Debug; // trace, verbose and debug
		}

		// SDL's log output function: SDL_Log and friends end up in Log. It stays installed (restoring SDL's own
		// function needs a pointer MSVC can't see is noexcept), and writes to SDL's default output while no
		// session exists. Log takes the state mutex, so nothing that holds it may log through SDL (it would
		// deadlock).
		void SDLCALL WriteSdlMessage(void*, int category, SDL_LogPriority priority, const char* message) noexcept
		{
			const char* text = message != nullptr ? message : "";
			if (LogIsActive())
			{
				LogAt(FromSdl(priority), text, {}, 0); // SDL's own call site is no use
			}
			else
			{
				SDL_GetDefaultLogOutputFunction()(nullptr, category, priority, text);
			}
		}
	} // namespace

	void RouteSdlLog()
	{
		SDL_SetLogOutputFunction(&WriteSdlMessage, nullptr);
	}
} // namespace engine::core
