#include "core/assert.hpp"

#include <cstdio>
#include <cstdlib>

namespace engine
{
	namespace
	{
		void DefaultAssertHandler(const char* expression, const char* message, const char* file, int line)
		{
			std::fprintf(stderr, "Assertion failed: %s\n  %s\n  at %s:%d\n", expression, message, file, line);
			std::fflush(stderr);
			std::abort();
		}

		AssertHandler g_assertHandler = &DefaultAssertHandler;
	}

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
}
