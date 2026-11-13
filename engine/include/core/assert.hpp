#pragma once

// ENGINE_ASSERT(condition, message)
//
// Checks an invariant. When the condition is false, the current assert handler
// is called. The default handler prints the failure and aborts; tests install a
// handler that throws so a failed assert can be checked with CHECK_THROWS.
//
// Asserts are compiled in when ENGINE_ENABLE_ASSERTS is defined (Debug builds by
// default, see engine/CMakeLists.txt) and compile to nothing otherwise.

#include <cstdlib>

namespace engine
{
	using AssertHandler = void (*)(const char* expression, const char* message, const char* file, int line);

	// Replaces the assert handler and returns the previous one.
	// Passing nullptr restores the default handler.
	AssertHandler SetAssertHandler(AssertHandler handler) noexcept;

	// Called by ENGINE_ASSERT; forwards to the current handler.
	void ReportAssertFailure(const char* expression, const char* message, const char* file, int line);

#if defined(ENGINE_ENABLE_ASSERTS)
	inline constexpr bool kAssertsEnabled = true;
#else
	inline constexpr bool kAssertsEnabled = false;
#endif
} // namespace engine

// ENGINE_VERIFY(condition, message)
//
// Like ENGINE_ASSERT, but compiled in every configuration. Use it only where continuing
// would corrupt memory (e.g. an out-of-range write), so a cheap check is worth keeping in
// Release. If the handler returns instead of aborting or throwing, the process aborts anyway.
// clang-format off: the macros are aligned by hand.
#define ENGINE_VERIFY(condition, message)                                                  \
	do                                                                                     \
	{                                                                                      \
		if (!(condition))                                                                  \
		{                                                                                  \
			::engine::ReportAssertFailure(#condition, (message), __FILE__, __LINE__);     \
			std::abort();                                                                  \
		}                                                                                  \
	} while (false)

#if defined(ENGINE_ENABLE_ASSERTS)
	#define ENGINE_ASSERT(condition, message)                                            \
		do                                                                                 \
		{                                                                                  \
			if (!(condition))                                                              \
			{                                                                              \
				::engine::ReportAssertFailure(#condition, (message), __FILE__, __LINE__); \
			}                                                                              \
		} while (false)
#else
	#define ENGINE_ASSERT(condition, message) ((void)0)
#endif
// clang-format on
