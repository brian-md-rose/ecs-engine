#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL.h>

#include <cstdlib>

#include "application/application.hpp"
#include "application/editorhost.hpp"
#include "editor/editor.hpp"

// A live smoke test of the editor overlay: the real Application and the real Editor, on SDL's offscreen
// video driver, for a few frames. If a machine can't make even that window, the tests skip.
namespace
{
	using engine::application::Application;
	using engine::application::ApplicationConfig;
	using engine::application::EditorHost;
	using engine::application::EditorSurface;
	using engine::application::Game;

	[[maybe_unused]] const bool kOffscreenChosen = []
	{
#ifdef _WIN32
		char* chosen = nullptr;
		std::size_t length = 0;
		if (_dupenv_s(&chosen, &length, "SDL_VIDEO_DRIVER") != 0 || chosen == nullptr)
		{
			_putenv_s("SDL_VIDEO_DRIVER", "offscreen");
		}
		std::free(chosen);
#else
		setenv("SDL_VIDEO_DRIVER", "offscreen", 0);
#endif
		return true;
	}();

	// Runs for a few updates, pressing F1 on the second, then asks to quit.
	class ScriptedGame final : public Game
	{
	public:
		void Update(double, const engine::input::Input&, engine::audio::AudioDevice*) override
		{
			++updates;
			if (updates == 2)
			{
				SDL_Event event{};
				event.type = SDL_EVENT_KEY_DOWN;
				event.key.scancode = SDL_SCANCODE_F1;
				event.key.key = SDLK_F1;
				event.key.down = true;
				SDL_PushEvent(&event);
			}
		}
		void Render(engine::graphics::Renderer&, double) override { ++renders; }
		[[nodiscard]] bool WantsToQuit() const override { return updates >= 6; }

		int updates = 0;
		int renders = 0;
	};

	class SpyHost final : public EditorHost
	{
	public:
		void Attach(const EditorSurface& surface) override
		{
			++attaches;
			sawHandles = surface.window != nullptr && surface.renderer != nullptr;
		}
		void Detach() override { ++detaches; }
		bool OnEvent(const void*) override
		{
			++events;
			return false;
		}
		void Draw() override { ++draws; }

		int attaches = 0;
		int detaches = 0;
		int events = 0;
		int draws = 0;
		bool sawHandles = false;
	};

	ApplicationConfig TestConfig()
	{
		ApplicationConfig config;
		config.title = "editor test";
		config.vsync = false;
		config.frameCap = 0;
		return config;
	}
} // namespace

TEST_CASE("Application calls its editor host around the main loop", "[editor][application]")
{
	Application app(TestConfig());
	ScriptedGame game;
	SpyHost host;
	app.SetEditorHost(&host);
	if (app.Run(game) != 0) SKIP("no offscreen video driver");

	CHECK(host.attaches == 1);
	CHECK(host.detaches == 1);
	CHECK(host.sawHandles);
	CHECK(host.draws == game.renders);
	CHECK(host.draws > 0);
	CHECK(host.events >= 1); // the F1 press at least
}

TEST_CASE("The editor overlay toggles with F1 and draws without trouble", "[editor][application]")
{
	Application app(TestConfig());
	ScriptedGame game;
	engine::editor::Editor editor;
	int panelCalls = 0;
	editor.AddPanel("Test", [&panelCalls]
	{
		++panelCalls;
	});
	app.SetEditorHost(&editor);
	if (app.Run(game) != 0) SKIP("no offscreen video driver");

	CHECK(editor.IsVisible()); // F1 was pressed once
	CHECK(panelCalls == 0);    // panels start closed
	CHECK(game.renders > 0);
}
