#pragma once

namespace engine::core
{
	// Turns variable frame times into a whole number of fixed-size updates.
	//
	// Each frame, pass the real time that elapsed to Advance(); run the returned number of
	// updates, each with Step() seconds. Leftover time carries over to the next frame, so
	// game logic runs at the same rate on every machine regardless of frame rate.
	class FixedTimestep
	{
		double step;
		double maxFrame;
		double accumulator = 0.0;

	public:
		// stepSeconds: length of one update (default 60 Hz).
		// maxFrameSeconds: longest frame that is honoured. After a stall (breakpoint, window
		// drag) the game slows down briefly instead of running hundreds of catch-up updates.
		explicit FixedTimestep(double stepSeconds = 1.0 / 60.0, double maxFrameSeconds = 0.25);

		// Adds elapsed real time; returns how many fixed updates to run now.
		// Negative times (a clock that went backwards) are treated as zero.
		[[nodiscard]] int Advance(double frameSeconds);

		// How far the game is into the next update, in [0, 1). Renderers can use it to draw
		// positions between the last two updates.
		[[nodiscard]] double Alpha() const noexcept { return accumulator / step; }

		[[nodiscard]] double Step() const noexcept { return step; }
	};
} // namespace engine::core
