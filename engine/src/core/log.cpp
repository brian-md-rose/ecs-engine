#include "core/log.hpp"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_time.h>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>

#include "core/assert.hpp"
#include "core/text.hpp"

namespace engine::core
{
	namespace
	{
		struct LogState
		{
			std::mutex mutex;
			bool active = false;
			bool console = true;
			LogLevel minimumLevel = LogLevel::Info;
			std::ofstream file;
		};

		LogState& State()
		{
			static LogState state;
			return state;
		}

		const char* LevelName(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::Debug:
				return "DEBUG";
			case LogLevel::Info:
				return "INFO ";
			case LogLevel::Warning:
				return "WARN ";
			case LogLevel::Error:
				return "ERROR";
			}
			return "?????";
		}

		// An if chain, not a switch: SDL_LogPriority has enumerators we have no use for, and /Wall warns about
		// every one a switch leaves unhandled.
		LogLevel FromSdl(SDL_LogPriority priority)
		{
			if (priority == SDL_LOG_PRIORITY_WARN) return LogLevel::Warning;
			if (priority == SDL_LOG_PRIORITY_ERROR || priority == SDL_LOG_PRIORITY_CRITICAL) return LogLevel::Error;
			if (priority == SDL_LOG_PRIORITY_INFO) return LogLevel::Info;
			return LogLevel::Debug; // trace, verbose and debug
		}

		// "2026-11-18 16:09:27.123" in local time, or "-" if SDL can't say.
		std::string Timestamp()
		{
			SDL_Time now = 0;
			SDL_DateTime time{};
			if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &time, true)) return "-";
			char text[40];
			std::snprintf(text, sizeof text, "%04d-%02d-%02d %02d:%02d:%02d.%03d", time.year, time.month, time.day,
			              time.hour, time.minute, time.second, time.nanosecond / 1'000'000);
			return text;
		}

		bool IsActive()
		{
			LogState& state = State();
			const std::lock_guard<std::mutex> lock(state.mutex);
			return state.active;
		}

		// SDL's log output function: SDL_Log and friends end up in Log. It stays installed once a session has
		// been made (restoring SDL's own function needs a pointer MSVC can't see is noexcept), and writes to
		// SDL's default output while no session exists. Log takes the state mutex, so nothing that holds it
		// may log through SDL (it would deadlock).
		void SDLCALL WriteSdlMessage(void*, int category, SDL_LogPriority priority, const char* message) noexcept
		{
			const char* text = message != nullptr ? message : "";
			if (IsActive())
			{
				Log(FromSdl(priority), text);
			}
			else
			{
				SDL_GetDefaultLogOutputFunction()(nullptr, category, priority, text);
			}
		}
	} // namespace

	LogSession::LogSession(const LogConfig& config)
	{
		LogState& state = State();
		std::string problem;
		bool overlapping = false;
		{
			const std::lock_guard<std::mutex> lock(state.mutex);
			state.minimumLevel = config.minimumLevel;
			state.console = config.console;
			state.file.close();
			if (!config.file.empty())
			{
				std::error_code error;
				if (std::filesystem::exists(config.file, error))
				{
					std::filesystem::path old = config.file;
					old.replace_filename(config.file.stem().native() + std::filesystem::path(".old").native() +
					                     config.file.extension().native());
					std::filesystem::rename(config.file, old, error); // replaces old if it exists
				}
				if (config.file.has_parent_path())
					std::filesystem::create_directories(config.file.parent_path(), error);
				state.file.open(config.file, std::ios::binary | std::ios::trunc);
				if (!state.file) problem = PathText(config.file) + ": can't open the log file";
			}
			overlapping = state.active;
			SDL_SetLogOutputFunction(&WriteSdlMessage, nullptr);
			state.active = true;
		}
		// After the lock: the assert handler logs, and the mutex isn't recursive.
		ENGINE_ASSERT(!overlapping, "only one LogSession can exist at a time");
		static_cast<void>(overlapping); // unused when asserts are compiled out
		if (!problem.empty()) Log(LogLevel::Error, problem);
		if (!config.file.empty()) Log(LogLevel::Info, "Logging to " + PathText(config.file));
	}

	LogSession::~LogSession()
	{
		LogState& state = State();
		const std::lock_guard<std::mutex> lock(state.mutex);
		state.file.close();
		state.active = false;
	}

	void Log(LogLevel level, std::string_view message)
	{
		LogState& state = State();
		const std::lock_guard<std::mutex> lock(state.mutex);
		if (state.active && level < state.minimumLevel) return;

		std::string line = Timestamp();
		line += " [";
		line += LevelName(level);
		line += "] ";
		line += message;
		line += '\n';

		// Without a session, or when there's nowhere else to write, the console it is.
		if (!state.active || state.console || !state.file.is_open())
		{
			std::fwrite(line.data(), 1, line.size(), stderr);
		}
		if (state.file.is_open())
		{
			state.file << line;
			state.file.flush(); // so a crash loses nothing
		}
	}
} // namespace engine::core
