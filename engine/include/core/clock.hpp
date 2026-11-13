#pragma once

namespace engine::core
{
	// Source of time for the main loop. An interface so tests can drive time by hand.
	class Clock
	{
	public:
		virtual ~Clock() = default;

		// Seconds since an arbitrary fixed point. Never decreases.
		[[nodiscard]] virtual double Now() const = 0;
	};

	// The real clock, backed by std::chrono::steady_clock.
	class SystemClock final : public Clock
	{
	public:
		[[nodiscard]] double Now() const override;
	};
} // namespace engine::core
