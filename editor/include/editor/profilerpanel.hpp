#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "ecs/registry.hpp"
#include "editor/editor.hpp"

namespace engine::ecs
{
	struct World;
}

namespace engine::editor
{
	// One frame as the Profiler sees it.
	struct ProfilerSample
	{
		float frameMs = 0.0F;  // the whole frame, from the previous one
		float updateMs = 0.0F; // of that, the game's Update (0 while nothing runs)
		std::size_t entities = 0;
	};

	// The last `capacity` frames, oldest first. Plain data, so it is tested without a window.
	class ProfilerHistory
	{
		std::vector<ProfilerSample> samples; // a ring: `next` is the oldest once full
		std::size_t next = 0;
		std::size_t capacity;

	public:
		explicit ProfilerHistory(std::size_t frames = 240) : capacity(frames == 0 ? 1 : frames) {}

		void Add(const ProfilerSample& sample);
		void Clear() noexcept
		{
			samples.clear();
			next = 0;
		}
		[[nodiscard]] std::size_t Size() const noexcept { return samples.size(); }
		[[nodiscard]] std::size_t Capacity() const noexcept { return capacity; }

		// The samples, oldest first.
		[[nodiscard]] std::vector<ProfilerSample> Ordered() const;
	};

	// Numbers over a run of frames.
	struct ProfilerSummary
	{
		float averageFrameMs = 0.0F;
		float maxFrameMs = 0.0F;
		float averageUpdateMs = 0.0F;
		float fps = 0.0F; // from the average frame time; 0 without frames
	};
	[[nodiscard]] ProfilerSummary Summarize(const std::vector<ProfilerSample>& samples);

	// The component counts, biggest first (ties by name).
	[[nodiscard]] std::vector<ecs::Registry::ComponentCount> CountsByPopulation(
	    std::vector<ecs::Registry::ComponentCount> counts);

	// What the game's Update costs, handed from the editor's Update to the panel's frame.
	class Profiler
	{
		double pendingUpdateMs = 0.0;

	public:
		// Adds time spent in one Update call; the panel takes the total at its next frame.
		void AddUpdateMs(double ms) noexcept { pendingUpdateMs += ms; }
		[[nodiscard]] double TakeUpdateMs() noexcept
		{
			const double ms = pendingUpdateMs;
			pendingUpdateMs = 0.0;
			return ms;
		}
	};

	// Adds the Profiler panel: a frame-time graph (the last 240 frames, with the update share), average, worst and
	// FPS, the entity count and a table of how many entities have each component. Frames are only recorded while
	// the panel is open. `world` gives the world to count (the played one when playing). Returns the Profiler the
	// caller feeds with Update times (may be ignored: the update share then reads 0).
	std::shared_ptr<Profiler> AddProfilerPanel(Editor& editor, std::function<ecs::World&()> world);
} // namespace engine::editor
