#include "editor/editor.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <cstddef>
#include <utility>

#include "core/version.hpp"

namespace engine::editor
{
	namespace
	{
		constexpr SDL_Scancode kToggleKey = SDL_SCANCODE_F1;

		bool IsMouseEvent(Uint32 type)
		{
			return type == SDL_EVENT_MOUSE_MOTION || type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
			       type == SDL_EVENT_MOUSE_BUTTON_UP || type == SDL_EVENT_MOUSE_WHEEL;
		}

		bool IsKeyboardEvent(Uint32 type)
		{
			return type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP || type == SDL_EVENT_TEXT_INPUT;
		}
	} // namespace

	Editor::Editor() = default;

	Editor::~Editor()
	{
		if (attached) Detach();
	}

	void Editor::AddPanel(std::string name, PanelFunction draw)
	{
		panels.push_back({std::move(name), std::move(draw), false});
	}

	bool Editor::SetPanelOpen(std::string_view name, bool open)
	{
		for (Panel& panel : panels)
		{
			if (panel.name != name) continue;
			panel.open = open;
			return true;
		}
		return false;
	}

	void Editor::Attach(const application::EditorSurface& surface)
	{
		if (attached) return;
		SDL_Window* const window = static_cast<SDL_Window*>(surface.window);
		SDL_Renderer* const sdlRenderer = static_cast<SDL_Renderer*>(surface.renderer);
		if (window == nullptr || sdlRenderer == nullptr) return;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr; // no imgui.ini next to wherever the game was started from
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();
		if (!ImGui_ImplSDL3_InitForSDLRenderer(window, sdlRenderer))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "the editor's SDL backend failed to start");
			ImGui::DestroyContext();
			return;
		}
		if (!ImGui_ImplSDLRenderer3_Init(sdlRenderer))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "the editor's renderer backend failed to start");
			ImGui_ImplSDL3_Shutdown();
			ImGui::DestroyContext();
			return;
		}
		renderer = sdlRenderer;
		attached = true;
	}

	void Editor::Detach()
	{
		if (!attached) return;
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		renderer = nullptr;
		attached = false;
	}

	bool Editor::OnEvent(const void* sdlEvent)
	{
		if (!attached || sdlEvent == nullptr) return false;
		const SDL_Event& event = *static_cast<const SDL_Event*>(sdlEvent);

		if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.scancode == kToggleKey)
		{
			if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) visible = !visible;
			return true; // both halves of the press, so the game never sees a lone release
		}
		if (!visible) return false;

		ImGui_ImplSDL3_ProcessEvent(&event);
		const ImGuiIO& io = ImGui::GetIO();
		// Releases always reach the game too, so a key or button it saw go down is never left held when the
		// overlay takes the release. A release for a press it never saw is harmless to it.
		if (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_MOUSE_BUTTON_UP) return false;
		if (IsMouseEvent(event.type)) return io.WantCaptureMouse;
		if (IsKeyboardEvent(event.type)) return io.WantCaptureKeyboard;
		return false; // quit, window and controller events always reach the game
	}

	void Editor::Draw()
	{
		if (!attached || !visible) return;
		SDL_Renderer* const sdlRenderer = static_cast<SDL_Renderer*>(renderer);

		// The game draws in a logical size, scaled and letterboxed to the window. The overlay draws in real window
		// pixels (and gets the events in them), so its text stays sharp whatever the game's size is.
		int logicalWidth = 0;
		int logicalHeight = 0;
		SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
		SDL_GetRenderLogicalPresentation(sdlRenderer, &logicalWidth, &logicalHeight, &mode);
		SDL_SetRenderLogicalPresentation(sdlRenderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		DrawUi();
		ImGui::Render();
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdlRenderer);

		SDL_SetRenderLogicalPresentation(sdlRenderer, logicalWidth, logicalHeight, mode);
	}

	void Editor::DrawUi()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Panels"))
			{
				for (Panel& panel : panels)
					ImGui::MenuItem(panel.name.c_str(), nullptr, &panel.open);
				if (!panels.empty()) ImGui::Separator();
				ImGui::MenuItem("ImGui demo", nullptr, &showImGuiDemo);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help"))
			{
				ImGui::MenuItem("About", nullptr, &showAbout);
				ImGui::TextDisabled("F1 hides the editor");
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		// By index, since a panel may add panels (the deque keeps the ones already there in place).
		for (std::size_t i = 0; i < panels.size(); ++i)
		{
			if (!panels[i].open) continue;
			if (ImGui::Begin(panels[i].name.c_str(), &panels[i].open)) panels[i].draw();
			ImGui::End();
		}
		if (showAbout) DrawAbout();
		if (showImGuiDemo) ImGui::ShowDemoWindow(&showImGuiDemo);
	}

	void Editor::DrawAbout()
	{
		if (ImGui::Begin("About", &showAbout, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("ecs-engine editor overlay");
			ImGui::Text("engine %s", kVersionString);
			ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
			ImGui::Text("%.1f frames per second", static_cast<double>(ImGui::GetIO().Framerate));
		}
		ImGui::End();
	}
} // namespace engine::editor
