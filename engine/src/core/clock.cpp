#include "core/clock.hpp"

#include <chrono>

namespace engine::core
{
	double SystemClock::Now() const
	{
		using Seconds = std::chrono::duration<double>;
		return std::chrono::duration_cast<Seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
} // namespace engine::core
