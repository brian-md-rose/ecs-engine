#pragma once
#include <stdexcept>
#include <string>

#include "core/assert.hpp"

namespace engine::test
{
	// Thrown instead of aborting when an ENGINE_ASSERT fails inside a test.
	struct AssertFailure : std::runtime_error
	{
		explicit AssertFailure(const std::string& what) : std::runtime_error(what) {}
	};

	// While alive, failed ENGINE_ASSERTs throw AssertFailure so tests can use
	// CHECK_THROWS_AS(..., AssertFailure). Restores the previous handler on exit.
	class AssertsThrow
	{
		AssertHandler previous;

		static void Throw(const char* expression, const char* message, const char* file, int line)
		{
			throw AssertFailure(std::string(expression) + " (" + message + ") at " + file + ":" + std::to_string(line));
		}

	public:
		AssertsThrow() noexcept : previous(SetAssertHandler(&Throw)) {}
		~AssertsThrow() { SetAssertHandler(previous); }

		AssertsThrow(const AssertsThrow&) = delete;
		AssertsThrow& operator=(const AssertsThrow&) = delete;
	};
} // namespace engine::test
