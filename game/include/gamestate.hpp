#pragma once
#include <chrono>
#include <cstddef>

namespace game
{
	// Game-wide flags, kept as a resource.
	struct GameState
	{
		bool paused = false;
		bool stressTest = false;
	};

	// Resource: prints frames per second and the average time spent drawing, once a second,
	// while the stress test runs.
	class FrameStats
	{
		using Clock = std::chrono::steady_clock;

		Clock::time_point windowStart = Clock::now();
		Clock::duration drawTime{};
		int frames = 0;

	public:
		void Reset();
		void AddFrame(Clock::duration draw, std::size_t shapes);
	};
} // namespace game
