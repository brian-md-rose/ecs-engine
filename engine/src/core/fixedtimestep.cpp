#include "core/fixedtimestep.hpp"

#include <algorithm>

#include "core/assert.hpp"

namespace engine::core
{
	FixedTimestep::FixedTimestep(double stepSeconds, double maxFrameSeconds)
	    : step(stepSeconds), maxFrame(maxFrameSeconds)
	{
		ENGINE_ASSERT(stepSeconds > 0.0, "fixed step must be positive");
		ENGINE_ASSERT(maxFrameSeconds >= stepSeconds, "max frame must allow at least one update");
	}

	int FixedTimestep::Advance(double frameSeconds)
	{
		accumulator += std::clamp(frameSeconds, 0.0, maxFrame);

		int updates = 0;
		while (accumulator >= step)
		{
			accumulator -= step;
			++updates;
		}
		return updates;
	}
} // namespace engine::core
