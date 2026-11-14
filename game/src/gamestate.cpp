#include "gamestate.hpp"

#include <cstdio>

namespace game
{
	void FrameStats::Reset()
	{
		windowStart = Clock::now();
		drawTime = {};
		frames = 0;
	}

	void FrameStats::AddFrame(Clock::duration draw, std::size_t shapes)
	{
		drawTime += draw;
		++frames;

		const std::chrono::duration<double> elapsed = Clock::now() - windowStart;
		if (elapsed.count() < 1.0)
		{
			return;
		}

		const std::chrono::duration<double, std::milli> averageDraw = drawTime / frames;
		std::printf("stress test: %zu shapes, %.1f fps, draw %.3f ms/frame\n", shapes,
		            static_cast<double>(frames) / elapsed.count(), averageDraw.count());
		std::fflush(stdout);
		Reset();
	}
} // namespace game
