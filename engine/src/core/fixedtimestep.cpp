#include "core/fixedtimestep.hpp"

#include <algorithm>
#include <cmath>

#include "core/assert.hpp"

namespace engine::core
{
	FixedTimestep::FixedTimestep(double stepSeconds, double maxFrameSeconds)
	    : step(stepSeconds), maxFrame(maxFrameSeconds)
	{
		ENGINE_ASSERT(stepSeconds > 0.0, "fixed step must be positive");
		ENGINE_ASSERT(maxFrameSeconds >= stepSeconds, "max frame must allow at least one update");
	}

	void FixedTimestep::SetTimeScale(double scale)
	{
		ENGINE_ASSERT(scale >= 0.0, "time scale must not be negative");
		timeScale = std::max(scale, 0.0);
	}

	int FixedTimestep::Advance(double frameSeconds)
	{
		if (paused)
		{
			return 0;
		}
		accumulator += std::clamp(frameSeconds, 0.0, maxFrame) * timeScale;

		int updates = 0;
		while (accumulator >= step)
		{
			if (maxUpdates > 0 && updates >= maxUpdates)
			{
				accumulator = std::fmod(accumulator, step); // give up the backlog
				break;
			}
			accumulator -= step;
			++updates;
		}
		return updates;
	}
} // namespace engine::core
