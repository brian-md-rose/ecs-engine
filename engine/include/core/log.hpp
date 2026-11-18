#pragma once
#include <filesystem>
#include <string_view>

// Logging for the engine and the games built on it. Messages go to a log file, to the console
// (stderr), or both, each with a time and a level. SDL-free, so it is part of engine::core. The platform
// library can also route SDL's own log (SDL_Log, SDL_LogError, ...) through here; see core/sdllog.hpp.
//
// Without a LogSession, messages just go to stderr, so the engine and the tests need no setup.
// A game makes one LogSession in main(), before it makes anything that might fail.
namespace engine::core
{
	enum class LogLevel
	{
		Debug,
		Info,
		Warning,
		Error,
	};

	struct LogConfig
	{
		// The log file, or empty for no file. An existing file is first renamed to name.old.ext
		// (replacing the previous .old), so the last two runs are kept. If the file can't be
		// opened, the reason is logged and the messages go to the console instead.
		std::filesystem::path file;

		// Messages below this level are dropped.
		LogLevel minimumLevel = LogLevel::Info;

		// Also write to stderr. On by default in Debug builds only, which are the builds with a
		// console window (the game is a GUI app in the others, where stderr goes nowhere).
#ifdef NDEBUG
		bool console = false;
#else
		bool console = true;
#endif
	};

	// Logs while it exists. Only one at a time.
	class LogSession
	{
	public:
		explicit LogSession(const LogConfig& config);
		~LogSession();
		LogSession(const LogSession&) = delete;
		LogSession& operator=(const LogSession&) = delete;

	private:
		bool owns = false; // false for a session made while another exists, which does nothing
	};

	// True while a LogSession exists.
	[[nodiscard]] bool LogIsActive();

	// Writes one line, "2026-11-18 16:09:27.123 [ERROR] message". Safe to call from any thread.
	void Log(LogLevel level, std::string_view message);

	inline void LogDebug(std::string_view message)
	{
		Log(LogLevel::Debug, message);
	}
	inline void LogInfo(std::string_view message)
	{
		Log(LogLevel::Info, message);
	}
	inline void LogWarning(std::string_view message)
	{
		Log(LogLevel::Warning, message);
	}
	inline void LogError(std::string_view message)
	{
		Log(LogLevel::Error, message);
	}
} // namespace engine::core
