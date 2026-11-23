#include "editor/consolepanel.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace engine::editor
{
	namespace
	{
		const char* LevelLabel(core::LogLevel level)
		{
			switch (level)
			{
			case core::LogLevel::Debug:
				return "DEBUG";
			case core::LogLevel::Info:
				return "INFO";
			case core::LogLevel::Warning:
				return "WARN";
			case core::LogLevel::Error:
				return "ERROR";
			}
			return "?";
		}

		ImVec4 LevelColour(core::LogLevel level)
		{
			switch (level)
			{
			case core::LogLevel::Debug:
				return {0.55F, 0.55F, 0.6F, 1.0F};
			case core::LogLevel::Info:
				return {0.88F, 0.88F, 0.88F, 1.0F};
			case core::LogLevel::Warning:
				return {1.0F, 0.8F, 0.3F, 1.0F};
			case core::LogLevel::Error:
				return {1.0F, 0.4F, 0.4F, 1.0F};
			}
			return {1.0F, 1.0F, 1.0F, 1.0F};
		}

		bool ContainsNoCase(const std::string& text, const std::string& needle)
		{
			const auto same = [](char a, char b)
			{
				return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			};
			return std::search(text.begin(), text.end(), needle.begin(), needle.end(), same) != text.end();
		}

		struct ConsoleState
		{
			ConsoleFilter filter;
			bool autoScroll = true;
			std::vector<core::LogRecord> records; // a copy of the log history, refreshed when it changes
			std::uint64_t version = 0;
			bool loaded = false;
			std::size_t counts[4] = {};
			std::vector<std::size_t> shown; // indices into records that pass the filter
			ConsoleFilter shownFor;         // the filter `shown` was built with
			bool shownValid = false;
		};

		bool& LevelFlag(ConsoleFilter& filter, core::LogLevel level)
		{
			switch (level)
			{
			case core::LogLevel::Debug:
				return filter.debug;
			case core::LogLevel::Info:
				return filter.info;
			case core::LogLevel::Warning:
				return filter.warning;
			case core::LogLevel::Error:
				return filter.error;
			}
			return filter.info;
		}

		void DrawConsole(ConsoleState& state)
		{
			const std::uint64_t version = core::LogHistoryVersion();
			if (!state.loaded || version != state.version)
			{
				state.records = core::LogHistory();
				state.version = version;
				state.loaded = true;
				state.shownValid = false;
				for (std::size_t& count : state.counts)
					count = 0;
				for (const core::LogRecord& record : state.records)
				{
					const std::size_t index = static_cast<std::size_t>(record.level);
					if (index < std::size(state.counts)) ++state.counts[index];
				}
			}

			if (ImGui::Button("Clear"))
			{
				core::ClearLogHistory();
				state.records.clear();
				state.loaded = false; // reload next frame, so a message logged meanwhile is not skipped
				state.shownValid = false;
				for (std::size_t& count : state.counts)
					count = 0;
			}
			ImGui::SameLine();
			const bool copyAll = ImGui::Button("Copy");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy the messages shown to the clipboard");
			ImGui::SameLine();
			ImGui::Checkbox("Auto-scroll", &state.autoScroll);
			for (const core::LogLevel level :
			     {core::LogLevel::Debug, core::LogLevel::Info, core::LogLevel::Warning, core::LogLevel::Error})
			{
				ImGui::SameLine();
				const std::string label = std::string(LevelLabel(level)) + " " +
				                          std::to_string(state.counts[static_cast<std::size_t>(level)]);
				ImGui::PushStyleColor(ImGuiCol_Text, LevelColour(level));
				ImGui::Checkbox(label.c_str(), &LevelFlag(state.filter, level));
				ImGui::PopStyleColor();
			}
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##consolefilter", "filter", &state.filter.text);

			// Filtering reads every record, so only redo it when the history or the filter changed.
			const ConsoleFilter& f = state.filter;
			const ConsoleFilter& was = state.shownFor;
			if (!state.shownValid || f.debug != was.debug || f.info != was.info || f.warning != was.warning ||
			    f.error != was.error || f.text != was.text)
			{
				state.shown.clear();
				for (std::size_t i = 0; i < state.records.size(); ++i)
					if (ConsoleMatches(state.records[i], state.filter)) state.shown.push_back(i);
				state.shownFor = state.filter;
				state.shownValid = true;
			}
			const std::vector<std::size_t>& shown = state.shown;

			if (copyAll)
			{
				std::string text;
				for (const std::size_t index : shown)
					text += ConsoleLine(state.records[index]) + '\n';
				ImGui::SetClipboardText(text.c_str());
			}

			if (!ImGui::BeginChild("##consolelines", {0, 0}, ImGuiChildFlags_Borders,
			                       ImGuiWindowFlags_HorizontalScrollbar))
			{
				ImGui::EndChild();
				return;
			}
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(shown.size()));
			while (clipper.Step())
			{
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
				{
					const core::LogRecord& record = state.records[shown[static_cast<std::size_t>(row)]];
					ImGui::PushStyleColor(ImGuiCol_Text, LevelColour(record.level));
					const std::string line = ConsoleLine(record);
					ImGui::TextUnformatted(line.c_str(), line.c_str() + line.size());
					ImGui::PopStyleColor();
					if (ImGui::BeginPopupContextItem(("##lineMenu" + std::to_string(record.id)).c_str()))
					{
						if (ImGui::MenuItem("Copy line")) ImGui::SetClipboardText(line.c_str());
						ImGui::EndPopup();
					}
				}
			}
			// Stay at the bottom while new messages arrive, unless the user has scrolled up.
			if (state.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0F) ImGui::SetScrollHereY(1.0F);
			ImGui::EndChild();
		}
	} // namespace

	bool ConsoleMatches(const core::LogRecord& record, const ConsoleFilter& filter)
	{
		bool levelOn = true;
		switch (record.level)
		{
		case core::LogLevel::Debug:
			levelOn = filter.debug;
			break;
		case core::LogLevel::Info:
			levelOn = filter.info;
			break;
		case core::LogLevel::Warning:
			levelOn = filter.warning;
			break;
		case core::LogLevel::Error:
			levelOn = filter.error;
			break;
		}
		return levelOn && (filter.text.empty() || ContainsNoCase(record.message, filter.text));
	}

	std::string ConsoleLine(const core::LogRecord& record)
	{
		// Drop the date: the Console is about this run.
		const std::size_t space = record.time.find(' ');
		const std::string clock = space == std::string::npos ? record.time : record.time.substr(space + 1);
		return clock + " [" + LevelLabel(record.level) + "] " + record.message;
	}

	void AddConsolePanel(Editor& editor, std::size_t capacity)
	{
		core::SetLogHistoryCapacity(capacity);
		auto state = std::make_shared<ConsoleState>();
		editor.AddPanel("Console", [state]
		{
			DrawConsole(*state);
		});
		editor.SetPanelSize("Console", 520.0F, 240.0F);
		editor.SetPanelDock("Console", Editor::DockArea::Bottom);
	}
} // namespace engine::editor
