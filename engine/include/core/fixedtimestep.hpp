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
		double timeScale = 1.0;
		bool paused = false;

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

		// Game speed: 1 is normal, 0.5 is slow motion, 2 is double speed. The update length
		// stays Step(); a different scale changes how many updates run per real second. The
		// stall cap applies to real time first, so a scale above 1 can run more than
		// maxFrameSeconds / Step() updates in one frame (at most scale times as many).
		void SetTimeScale(double scale);
		[[nodiscard]] double TimeScale() const noexcept { return timeScale; }

		// While paused, Advance() returns 0 and Alpha() stays where it was. Real time that
		// passes during the pause is dropped, not replayed on resume.
		void SetPaused(bool value) noexcept { paused = value; }
		[[nodiscard]] bool Paused() const noexcept { return paused; }
	};
} // namespace engine::core
