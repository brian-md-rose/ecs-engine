#include "editor/profilerpanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <string>
#include <utility>

#include "ecs/world.hpp"

namespace engine::editor
{
	void ProfilerHistory::Add(const ProfilerSample& sample)
	{
		if (samples.size() < capacity)
		{
			samples.push_back(sample);
			return;
		}
		samples[next] = sample;
		next = (next + 1) % capacity;
	}

	std::vector<ProfilerSample> ProfilerHistory::Ordered() const
	{
		std::vector<ProfilerSample> ordered;
		ordered.reserve(samples.size());
		if (samples.size() < capacity)
		{
			ordered = samples;
			return ordered;
		}
		ordered.insert(ordered.end(), samples.begin() + static_cast<std::ptrdiff_t>(next), samples.end());
		ordered.insert(ordered.end(), samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(next));
		return ordered;
	}

	ProfilerSummary Summarize(const std::vector<ProfilerSample>& samples)
	{
		ProfilerSummary summary;
		if (samples.empty()) return summary;
		double frameTotal = 0.0;
		double updateTotal = 0.0;
		for (const ProfilerSample& sample : samples)
		{
			frameTotal += static_cast<double>(sample.frameMs);
			updateTotal += static_cast<double>(sample.updateMs);
			summary.maxFrameMs = std::max(summary.maxFrameMs, sample.frameMs);
		}
		const double count = static_cast<double>(samples.size());
		summary.averageFrameMs = static_cast<float>(frameTotal / count);
		summary.averageUpdateMs = static_cast<float>(updateTotal / count);
		if (summary.averageFrameMs > 0.0F) summary.fps = 1000.0F / summary.averageFrameMs;
		return summary;
	}

	std::vector<ecs::Registry::ComponentCount> CountsByPopulation(std::vector<ecs::Registry::ComponentCount> counts)
	{
		std::ranges::stable_sort(counts,
		                         [](const ecs::Registry::ComponentCount& a, const ecs::Registry::ComponentCount& b)
		{
			if (a.count != b.count) return a.count > b.count;
			return a.name < b.name;
		});
		return counts;
	}

	std::vector<ecs::Scheduler::SystemTiming> SlowestFirst(std::vector<ecs::Scheduler::SystemTiming> timings)
	{
		std::ranges::stable_sort(timings,
		                         [](const ecs::Scheduler::SystemTiming& a, const ecs::Scheduler::SystemTiming& b)
		{
			return a.averageMs > b.averageMs;
		});
		return timings;
	}

	namespace
	{
		struct ProfilerState
		{
			ProfilerHistory history;
			std::shared_ptr<Profiler> profiler = std::make_shared<Profiler>();
			bool paused = false;
			bool hideEmpty = true;
			bool slowestFirst = true; // the systems table order: slowest first, or as they run
			std::function<const ecs::Scheduler*()> scheduler;
		};

		// The per-system table: each system's smoothed time, for the game that is playing.
		void DrawSystems(ProfilerState& state)
		{
			const ecs::Scheduler* scheduler = state.scheduler ? state.scheduler() : nullptr;
			if (scheduler == nullptr)
			{
				ImGui::TextDisabled("Systems: press Play to see how long each one takes.");
				return;
			}
			std::vector<ecs::Scheduler::SystemTiming> timings = scheduler->Timings();
			double total = 0.0;
			for (const ecs::Scheduler::SystemTiming& timing : timings)
				total += timing.averageMs;
			if (state.slowestFirst) timings = SlowestFirst(std::move(timings));
			ImGui::Text("%zu systems, %.3f ms in all", timings.size(), total);
			ImGui::SameLine();
			ImGui::Checkbox("Slowest first", &state.slowestFirst);
			if (ImGui::BeginTable("##systems", 3,
			                      ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, {0.0F, 160.0F}))
			{
				ImGui::TableSetupColumn("System");
				ImGui::TableSetupColumn("Phase", ImGuiTableColumnFlags_WidthFixed, 70.0F);
				ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 60.0F);
				ImGui::TableHeadersRow();
				for (const ecs::Scheduler::SystemTiming& timing : timings)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					const std::string name(timing.name);
					if (timing.skipped)
						ImGui::TextDisabled("%s (skipped)", name.c_str());
					else
						ImGui::TextUnformatted(name.c_str());
					ImGui::TableNextColumn();
					const std::string phase(ecs::PhaseName(timing.phase));
					ImGui::TextUnformatted(phase.c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%.3f", timing.averageMs);
				}
				ImGui::EndTable();
			}
		}

		void DrawProfiler(ProfilerState& state, ecs::World& world)
		{
			const ImGuiIO& io = ImGui::GetIO();
			const float updateMs =
			    static_cast<float>(state.profiler->TakeUpdateMs(static_cast<long>(ImGui::GetFrameCount())));
			const std::size_t entities = world.registry.EntityCount();
			if (!state.paused)
				state.history.Add({.frameMs = io.DeltaTime * 1000.0F, .updateMs = updateMs, .entities = entities});

			if (ImGui::Button(state.paused ? "Resume" : "Pause")) state.paused = !state.paused;
			ImGui::SameLine();
			if (ImGui::Button("Clear")) state.history.Clear();
			ImGui::SameLine();
			ImGui::Checkbox("Hide empty", &state.hideEmpty);

			const std::vector<ProfilerSample> samples = state.history.Ordered();
			const ProfilerSummary summary = Summarize(samples);
			ImGui::Text("%.1f FPS   average %.2f ms   worst %.2f ms", static_cast<double>(summary.fps),
			            static_cast<double>(summary.averageFrameMs), static_cast<double>(summary.maxFrameMs));
			ImGui::Text("update %.2f ms   other (draw, editor) %.2f ms", static_cast<double>(summary.averageUpdateMs),
			            static_cast<double>(std::max(0.0F, summary.averageFrameMs - summary.averageUpdateMs)));

			std::vector<float> frame;
			std::vector<float> update;
			frame.reserve(samples.size());
			update.reserve(samples.size());
			for (const ProfilerSample& sample : samples)
			{
				frame.push_back(sample.frameMs);
				update.push_back(sample.updateMs);
			}
			const float scale = std::max(16.7F, summary.maxFrameMs);
			ImGui::PlotLines("##frame", frame.data(), static_cast<int>(frame.size()), 0, "frame ms", 0.0F, scale,
			                 {-FLT_MIN, 70.0F});
			ImGui::PlotLines("##update", update.data(), static_cast<int>(update.size()), 0, "update ms", 0.0F, scale,
			                 {-FLT_MIN, 40.0F});

			ImGui::Separator();
			DrawSystems(state);
			ImGui::Separator();
			ImGui::Text("%zu entities", entities);
			const std::vector<ecs::Registry::ComponentCount> counts =
			    CountsByPopulation(world.registry.ComponentCounts());
			if (ImGui::BeginTable("##components", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
			{
				ImGui::TableSetupColumn("Component");
				ImGui::TableSetupColumn("Entities", ImGuiTableColumnFlags_WidthFixed, 70.0F);
				ImGui::TableHeadersRow();
				for (const ecs::Registry::ComponentCount& count : counts)
				{
					if (state.hideEmpty && count.count == 0) continue;
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					const std::string name(count.name);
					ImGui::TextUnformatted(name.c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%zu", count.count);
				}
				ImGui::EndTable();
			}
		}
	} // namespace

	std::shared_ptr<Profiler> AddProfilerPanel(Editor& editor, std::function<ecs::World&()> world,
	                                              std::function<const ecs::Scheduler*()> scheduler)
	{
		auto state = std::make_shared<ProfilerState>();
		state->scheduler = std::move(scheduler);
		editor.AddPanel("Profiler", [state, world = std::move(world)]
		{
			DrawProfiler(*state, world());
		});
		return state->profiler;
	}
} // namespace engine::editor
