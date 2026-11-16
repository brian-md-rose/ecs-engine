#include "core/framestats.hpp"

namespace engine::core
{
	void FrameStats::Reset(Clock::time_point now)
	{
		windowStart = now;
		drawTime = {};
		frames = 0;
	}

	bool FrameStats::AddFrame(Clock::duration draw, std::size_t drawn, Clock::time_point now)
	{
		drawTime += draw;
		++frames;

		const std::chrono::duration<double> elapsed = now - windowStart;
		if (elapsed.count() < 1.0) return false;

		const std::chrono::duration<double, std::milli> averageDraw = drawTime / frames;
		fps = static_cast<double>(frames) / elapsed.count();
		drawMs = averageDraw.count();
		items = drawn;
		hasReport = true;
		Reset(now);
		return true;
	}
} // namespace engine::core
