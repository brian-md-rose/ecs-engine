#pragma once

#include <deque>
#include <functional>
#include <string>
#include <string_view>

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

		// Opens or closes the panel called name; false when there is none.
		bool SetPanelOpen(std::string_view name, bool open);

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
		};

		void DrawUi();
		void DrawAbout();

		void* renderer = nullptr; // the SDL_Renderer*, kept for Draw
		bool attached = false;
		bool visible = false;
		bool showAbout = false;
		bool showImGuiDemo = false;
		std::deque<Panel> panels; // a deque, so a panel adding a panel does not move the one drawing
	};
} // namespace engine::editor
