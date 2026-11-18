#include "core/assert.hpp"

#include <cstdlib>
#include <string>

#include "core/log.hpp"

namespace engine
{
	namespace
	{
		void DefaultAssertHandler(const char* expression, const char* message, const char* file, int line)
		{
			core::LogError(std::string("Assertion failed: ") + expression + "\n  " + message + "\n  at " + file + ":" +
			               std::to_string(line));
			std::abort();
		}

		AssertHandler g_assertHandler = &DefaultAssertHandler;
	} // namespace

	AssertHandler SetAssertHandler(AssertHandler handler) noexcept
	{
		const AssertHandler previous = g_assertHandler;
		g_assertHandler = handler != nullptr ? handler : &DefaultAssertHandler;
		return previous;
	}

	void ReportAssertFailure(const char* expression, const char* message, const char* file, int line)
	{
		g_assertHandler(expression, message, file, line);
	}
} // namespace engine
