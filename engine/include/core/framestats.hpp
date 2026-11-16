#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

namespace engine::core
{
	// A debug readout, kept as a resource: once a second, prints frames per second and the
	// average time spent drawing to stdout, as "label: N items, F fps, draw D ms/frame".
	class FrameStats
	{
		using Clock = std::chrono::steady_clock;

		std::string label;
		Clock::time_point windowStart = Clock::now();
		Clock::duration drawTime{};
		int frames = 0;

	public:
		explicit FrameStats(std::string_view name = "frame stats") : label(name) {}

		// Starts a new one-second window, forgetting the frames counted so far.
		void Reset();

		// Counts one frame that took draw to draw items things, and prints once a second has passed.
		void AddFrame(Clock::duration draw, std::size_t items);
	};
} // namespace engine::core
