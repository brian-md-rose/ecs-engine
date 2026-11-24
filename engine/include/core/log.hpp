#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

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

	// A remembered message, for a viewer such as the editor's Console panel.
	struct LogRecord
	{
		std::uint64_t id = 0; // 1, 2, 3, ... in the order logged; never reused, so a viewer can tell what is new
		LogLevel level = LogLevel::Info;
		std::string time;       // "2026-11-18 16:09:27.123"
		std::string message;    // without the time and level
		std::string file{};     // the source file of the call that logged it, as the compiler gave it; empty if unknown
		std::uint32_t line = 0; // its line, 0 if unknown
	};

	// Keeps the last `capacity` messages in memory (0, the default, keeps none and frees what was kept). Messages
	// dropped by LogConfig::minimumLevel are not kept while a LogSession is active; without one, all are kept.
	void SetLogHistoryCapacity(std::size_t capacity);

	// The remembered messages, oldest first. Safe to call from any thread.
	[[nodiscard]] std::vector<LogRecord> LogHistory();

	// A number that changes whenever the remembered messages do (a message kept, Clear, a new capacity), so a viewer
	// can skip copying LogHistory() on frames where nothing happened. Cheap.
	[[nodiscard]] std::uint64_t LogHistoryVersion();

	// Forgets the remembered messages (ids keep counting).
	void ClearLogHistory();

	// Writes one line, "2026-11-18 16:09:27.123 [ERROR] message". Safe to call from any thread. The file and line of
	// the call are kept in the LogRecord (not in the written line), so a viewer can jump to them.
	void Log(LogLevel level, std::string_view message, std::source_location where = std::source_location::current());

	// As Log, with the source location given by hand (empty file and line 0 when there is none), for a message that
	// is passed on from somewhere else, such as SDL's log.
	void LogAt(LogLevel level, std::string_view message, std::string_view file, std::uint32_t sourceLine);

	inline void LogDebug(std::string_view message, std::source_location where = std::source_location::current())
	{
		Log(LogLevel::Debug, message, where);
	}
	inline void LogInfo(std::string_view message, std::source_location where = std::source_location::current())
	{
		Log(LogLevel::Info, message, where);
	}
	inline void LogWarning(std::string_view message, std::source_location where = std::source_location::current())
	{
		Log(LogLevel::Warning, message, where);
	}
	inline void LogError(std::string_view message, std::source_location where = std::source_location::current())
	{
		Log(LogLevel::Error, message, where);
	}
} // namespace engine::core
