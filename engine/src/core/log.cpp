#include "core/log.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <deque>
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
			std::deque<LogRecord> history;
			std::size_t historyCapacity = 0;
			std::uint64_t nextId = 1;
			std::uint64_t version = 0;
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

		// "2026-11-18 16:09:27.123" in local time, or "-" if the clock can't say.
		std::string Timestamp()
		{
			const auto now = std::chrono::system_clock::now();
			const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
			std::tm local{};
#ifdef _WIN32
			if (localtime_s(&local, &seconds) != 0) return "-";
#else
			if (localtime_r(&seconds, &local) == nullptr) return "-";
#endif
			const auto millis =
			    std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
			char text[96]; // roomy: every int field could print 11 characters
			std::snprintf(text, sizeof text, "%04d-%02d-%02d %02d:%02d:%02d.%03d", local.tm_year + 1900,
			              local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec,
			              static_cast<int>(millis));
			return text;
		}
	} // namespace

	bool LogIsActive()
	{
		LogState& state = State();
		const std::lock_guard<std::mutex> lock(state.mutex);
		return state.active;
	}

	LogSession::LogSession(const LogConfig& config)
	{
		LogState& state = State();
		std::string problem;
		bool overlapping = false;
		{
			const std::lock_guard<std::mutex> lock(state.mutex);
			overlapping = state.active;
			if (!overlapping)
			{
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
				state.active = true;
			}
		}
		// After the lock: the assert handler logs, and the mutex isn't recursive.
		ENGINE_ASSERT(!overlapping, "only one LogSession can exist at a time");
		static_cast<void>(overlapping); // unused when asserts are compiled out
		owns = !overlapping;
		if (overlapping) return; // leave the running session alone
		if (!problem.empty()) Log(LogLevel::Error, problem);
		if (!config.file.empty()) Log(LogLevel::Info, "Logging to " + PathText(config.file));
	}

	LogSession::~LogSession()
	{
		if (!owns) return;
		LogState& state = State();
		const std::lock_guard<std::mutex> lock(state.mutex);
		state.file.close();
		state.active = false;
	}

	void SetLogHistoryCapacity(std::size_t capacity)
	{
		LogState& state = State();
		const std::lock_guard<std::mutex> lock(state.mutex);
		state.historyCapacity = capacity;
		++state.version;
		while (state.history.size() > capacity)
			state.history.pop_front();
		if (capacity == 0) state.history.shrink_to_fit();
	}

	std::uint64_t LogHistoryVersion()
	{
		LogState& state = State();
		const std::lock_guard<std::mutex> lock(state.mutex);
		return state.version;
	}

	std::vector<LogRecord> LogHistory()
	{
		LogState& state = State();
		const std::lock_guard<std::mutex> lock(state.mutex);
		return {state.history.begin(), state.history.end()};
	}

	void ClearLogHistory()
	{
		LogState& state = State();
		const std::lock_guard<std::mutex> lock(state.mutex);
		state.history.clear();
		++state.version;
	}

	void Log(LogLevel level, std::string_view message, std::source_location where)
	{
		LogAt(level, message, where.file_name(), where.line());
	}

	void LogAt(LogLevel level, std::string_view message, std::string_view file, std::uint32_t sourceLine)
	{
		LogState& state = State();
		const std::lock_guard<std::mutex> lock(state.mutex);
		if (state.active && level < state.minimumLevel) return;

		const std::string time = Timestamp();
		if (state.historyCapacity > 0)
		{
			if (state.history.size() >= state.historyCapacity) state.history.pop_front();
			state.history.push_back({state.nextId++, level, time, std::string(message), std::string(file), sourceLine});
			++state.version;
		}

		std::string line = time;
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
