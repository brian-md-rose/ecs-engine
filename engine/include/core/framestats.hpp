#pragma once

#include <chrono>
#include <cstddef>

namespace engine::core
{
	// Frames per second and the average time spent drawing, measured over one-second windows.
	// Kept as a resource; the debug panel feeds it every frame and shows the last full window.
	class FrameStats
	{
	public:
		using Clock = std::chrono::steady_clock;

	private:
		Clock::time_point windowStart = Clock::now();
		Clock::duration drawTime{};
		int frames = 0;

		double fps = 0.0;
		double drawMs = 0.0;
		std::size_t items = 0;
		bool hasReport = false;

	public:
		// Starts a new window, forgetting the frames counted so far. The last report is kept.
		void Reset(Clock::time_point now = Clock::now());

		// Counts one frame that took draw to draw items things. Once a second has passed since
		// the window started, updates the report and starts a new window, and returns true.
		bool AddFrame(Clock::duration draw, std::size_t drawn, Clock::time_point now = Clock::now());

		// The last full window: nothing until the first second has passed.
		[[nodiscard]] bool HasReport() const noexcept { return hasReport; }
		[[nodiscard]] double Fps() const noexcept { return fps; }
		[[nodiscard]] double AverageDrawMs() const noexcept { return drawMs; }
		[[nodiscard]] std::size_t Items() const noexcept { return items; } // drawn in the window's last frame
	};
} // namespace engine::core
