#pragma once

#include <deque>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "application/editorhost.hpp"

namespace engine::editor
{
	// A Dear ImGui overlay drawn on top of a running game. Give it to Application::SetEditorHost. F1 shows
	// and hides it; while it is shown, the mouse and keyboard go to its windows when the pointer or focus is
	// over one, and to the game otherwise.
	//
	// Panels are functions that call ImGui; each gets its own window, listed in the Panels menu. Calls
	// happen once per frame, between the game's Render and the frame being presented.
	class Editor final : public application::EditorHost
	{
	public:
		using PanelFunction = std::function<void()>;

		Editor();
		~Editor() override;

		Editor(const Editor&) = delete;
		Editor& operator=(const Editor&) = delete;
		Editor(Editor&&) = delete;
		Editor& operator=(Editor&&) = delete;

		// Adds a window called name that calls draw each frame while it is open. Panels start closed.
		// A panel may add more panels while it draws.
		void AddPanel(std::string name, PanelFunction draw);

		// Content for the main menu bar, drawn before the built-in Panels and Help menus: bar adds menus and items
		// (inside the bar), and dialogs draws popups, called right after, outside the bar. Either may be empty.
		void SetMenuBar(std::function<void()> bar, std::function<void()> dialogs = {})
		{
			menuBar = std::move(bar);
			menuDialogs = std::move(dialogs);
		}

		// Opens or closes the panel called name; false when there is none.
		bool SetPanelOpen(std::string_view name, bool open);

		// The size the panel called name opens at the first time (a saved layout wins later); false when there is none.
		bool SetPanelSize(std::string_view name, float width, float height);

		// Where Dear ImGui keeps the panels' positions and sizes between runs (an .ini file, written as they change
		// and read at the next start). Call before the editor is attached; empty, the default, keeps none.
		void SetLayoutFile(std::filesystem::path file) { layoutFile = std::move(file); }

		[[nodiscard]] bool IsVisible() const noexcept { return visible; }
		void SetVisible(bool shown) noexcept { visible = shown; }

		// EditorHost:
		void Attach(const application::EditorSurface& surface) override;
		void Detach() override;
		bool OnEvent(const void* sdlEvent) override;
		void Draw() override;
		[[nodiscard]] bool WantsSystemCursor() const override { return attached && visible; }

	private:
		struct Panel
		{
			std::string name;
			PanelFunction draw;
			bool open = false;
			float width = 0;
			float height = 0;
		};

		void DrawUi();
		void DrawAbout();

		void* renderer = nullptr;         // the SDL_Renderer*, kept for Draw
		std::filesystem::path layoutFile; // kept alive for ImGuiIO::IniFilename
		std::string layoutFileText;
		bool attached = false;
		bool visible = false;
		bool showAbout = false;
		bool showImGuiDemo = false;
		std::function<void()> menuBar;
		std::function<void()> menuDialogs;
		std::deque<Panel> panels; // a deque, so a panel adding a panel does not move the one drawing
	};
} // namespace engine::editor
