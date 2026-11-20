#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL.h>

#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "application/application.hpp"
#include "application/scheduledgame.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/name.hpp"
#include "ecs/typeregistry.hpp"
#include "editor/editor.hpp"
#include "editor/editorgame.hpp"
#include "editor/entityops.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"

// The keyboard shortcuts of the standalone editor, driven through real key events: the real Application, Editor and
// EditorGame on SDL's offscreen video driver. If a machine can't make even that window, the tests skip.
namespace
{
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

	using engine::ecs::Entity;
	using engine::editor::EditorGame;

	class TestGame final : public engine::application::ScheduledGame
	{
	public:
		TestGame()
		{
			auto& types = world.resources.Emplace<engine::ecs::TypeRegistry>();
			engine::ecs::RegisterEngineComponents(types);
		}
	};

	// Presses and releases a key, with modifiers held, as the window system would.
	void Press(SDL_Scancode scancode, SDL_Keycode key, SDL_Keymod mods = SDL_KMOD_NONE)
	{
		// The ImGui backend ignores a key event that is not for its window.
		SDL_WindowID window = 0;
		int windows = 0;
		if (SDL_Window** list = SDL_GetWindows(&windows))
		{
			if (windows > 0) window = SDL_GetWindowID(list[0]);
			SDL_free(list);
		}
		for (const bool down : {true, false})
		{
			SDL_Event event{};
			event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
			event.key.scancode = scancode;
			event.key.key = key;
			event.key.windowID = window;
			event.key.mod = mods;
			event.key.down = down;
			SDL_PushEvent(&event);
		}
	}

	// Runs script[n] after the n-th frame is drawn (counting from 1). A key pressed after frame n is handled while
	// frame n + 1 is drawn, so look at the result after frame n + 2.
	class Script final : public engine::application::Game
	{
	public:
		Script(EditorGame& gameToRun, int lastUpdate) : inner(gameToRun), last(lastUpdate) {}
		void Update(double dt, const engine::input::Input& input, engine::audio::AudioDevice* audio) override
		{
			inner.Update(dt, input, audio);
		}
		// Steps run after the frame is drawn, so they are counted in frames: updates can come several to a frame.
		void Render(engine::graphics::Renderer& renderer, double alpha) override
		{
			inner.Render(renderer, alpha);
			++frames;
			if (const auto step = script.find(frames); step != script.end()) step->second();
		}
		[[nodiscard]] bool WantsToQuit() const override { return frames >= last; }

		std::map<int, std::function<void()>> script;

	private:
		EditorGame& inner;
		int last;
		int frames = 0;
	};

	engine::application::ApplicationConfig TestConfig()
	{
		engine::application::ApplicationConfig config;
		config.title = "shortcut test";
		config.vsync = false;
		config.frameCap = 0;
		return config;
	}

	engine::editor::EditorGameConfig MakeConfig()
	{
		engine::editor::EditorGameConfig config;
		config.makeGame = []
		{
			return std::make_unique<TestGame>();
		};
		config.sceneFolder = std::filesystem::temp_directory_path() / "ecs_engine_shortcuts";
		return config;
	}
} // namespace

TEST_CASE("Ctrl+D, Delete, Ctrl+Z, Ctrl+C and Ctrl+V act on the selection", "[editor][shortcuts]")
{
	engine::application::Application app(TestConfig());
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig());
	REQUIRE(game.IsValid());
	app.SetEditorHost(&editor);

	auto count = [&game]
	{
		return game.EditWorld().registry.AllEntities().size();
	};
	std::vector<std::size_t> seen; // the entity count after each key
	Script run(game, 22);
	run.script[1] = [&]
	{
		const Entity box = engine::editor::CreateEntity(game.EditWorld().registry, "box");
		game.Selection().Select(box);
	};
	run.script[3] = []
	{
		Press(SDL_SCANCODE_D, SDLK_D, SDL_KMOD_CTRL);
	};
	run.script[6] = [&]
	{
		seen.push_back(count()); // duplicated: 2
		game.CheckDirty();       // each change becomes an undo step
		Press(SDL_SCANCODE_C, SDLK_C, SDL_KMOD_CTRL);
	};
	run.script[9] = []
	{
		Press(SDL_SCANCODE_V, SDLK_V, SDL_KMOD_CTRL);
	};
	run.script[12] = [&]
	{
		seen.push_back(count()); // pasted: 3
		game.CheckDirty();
		Press(SDL_SCANCODE_DELETE, SDLK_DELETE);
	};
	run.script[15] = [&]
	{
		seen.push_back(count()); // the pasted one deleted: 2
		game.CheckDirty();
		Press(SDL_SCANCODE_Z, SDLK_Z, SDL_KMOD_CTRL);
	};
	run.script[18] = [&]
	{
		seen.push_back(count());
	}; // undone: 3

	if (app.Run(run) != 0) SKIP("no offscreen video driver");
	REQUIRE(seen.size() == 4);
	CHECK(seen[0] == 2);
	CHECK(seen[1] == 3);
	CHECK(seen[2] == 2);
	CHECK(seen[3] == 3);
}

TEST_CASE("F5 plays and stops, and F6 restarts, and nothing else edits while playing", "[editor][shortcuts]")
{
	engine::application::Application app(TestConfig());
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig());
	REQUIRE(game.IsValid());
	app.SetEditorHost(&editor);

	std::vector<bool> playing;
	Script run(game, 20);
	run.script[1] = [&]
	{
		const Entity box = engine::editor::CreateEntity(game.EditWorld().registry, "box");
		game.Selection().Select(box);
	};
	run.script[3] = []
	{
		Press(SDL_SCANCODE_F5, SDLK_F5);
	};
	run.script[6] = [&]
	{
		playing.push_back(game.IsPlaying());          // true
		Press(SDL_SCANCODE_D, SDLK_D, SDL_KMOD_CTRL); // no duplicate while playing
	};
	run.script[9] = [&]
	{
		playing.push_back(game.EditWorld().registry.AllEntities().size() == 1);
		Press(SDL_SCANCODE_F6, SDLK_F6);
	};
	run.script[12] = [&]
	{
		playing.push_back(game.IsPlaying()); // still playing after Restart
		Press(SDL_SCANCODE_F5, SDLK_F5);
	};
	run.script[15] = [&]
	{
		playing.push_back(!game.IsPlaying());
	};

	if (app.Run(run) != 0) SKIP("no offscreen video driver");
	REQUIRE(playing.size() == 4);
	CHECK(playing[0]); // F5 started Play
	CHECK(playing[1]); // Ctrl+D did nothing while playing
	CHECK(playing[2]); // F6 left it playing
	CHECK(playing[3]); // F5 stopped it
}
