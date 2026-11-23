#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL.h>

#include <cstdlib>
#include <functional>

#include "application/application.hpp"
#include "application/editorhost.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/name.hpp"
#include "ecs/typeregistry.hpp"
#include "ecs/world.hpp"
#include "editor/editor.hpp"
#include "editor/entityops.hpp"
#include "editor/entitypanels.hpp"

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
			if (onUpdate) onUpdate(updates);
			if (pressF1 && updates == 2)
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
		bool pressF1 = true;
		std::function<void(int)> onUpdate; // called from each update, with the update's number
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

TEST_CASE("An open panel is drawn every frame and may add panels", "[editor][application]")
{
	Application app(TestConfig());
	ScriptedGame game;
	game.pressF1 = false;
	engine::editor::Editor editor;
	int panelCalls = 0;
	int innerCalls = 0;
	editor.AddPanel("Outer", [&]
	{
		++panelCalls;
		if (panelCalls == 1)
		{
			editor.AddPanel("Inner", [&innerCalls]
			{
				++innerCalls;
			});
			editor.SetPanelOpen("Inner", true);
		}
	});
	CHECK(editor.SetPanelOpen("Outer", true));
	CHECK_FALSE(editor.SetPanelOpen("Missing", true));
	editor.SetVisible(true);
	app.SetEditorHost(&editor);
	if (app.Run(game) != 0) SKIP("no offscreen video driver");

	CHECK(panelCalls == game.renders);
	CHECK(innerCalls == game.renders); // drawn in the frame it was added, too
}

TEST_CASE("The editor swallows F1 but never a quit request", "[editor][application]")
{
	Application app(TestConfig());
	ScriptedGame game;
	game.pressF1 = false;
	engine::editor::Editor editor;
	bool f1Taken = false;
	bool quitTaken = true;
	bool keyUpTaken = true;
	bool keyTakenWhileHidden = true;
	game.onUpdate = [&](int update)
	{
		if (update != 3) return;
		SDL_Event quit{};
		quit.type = SDL_EVENT_QUIT;
		editor.SetVisible(true); // so the quit goes through ImGui's event handling
		quitTaken = editor.OnEvent(&quit);
		SDL_Event keyUp{};
		keyUp.type = SDL_EVENT_KEY_UP;
		keyUp.key.scancode = SDL_SCANCODE_A;
		keyUpTaken = editor.OnEvent(&keyUp);
		SDL_Event f1{};
		f1.type = SDL_EVENT_KEY_DOWN;
		f1.key.scancode = SDL_SCANCODE_F1;
		f1Taken = editor.OnEvent(&f1);
		SDL_Event other{};
		other.type = SDL_EVENT_KEY_DOWN;
		other.key.scancode = SDL_SCANCODE_A;
		editor.SetVisible(false);
		keyTakenWhileHidden = editor.OnEvent(&other);
	};
	app.SetEditorHost(&editor);
	if (app.Run(game) != 0) SKIP("no offscreen video driver");

	CHECK_FALSE(quitTaken);
	CHECK_FALSE(keyUpTaken); // releases always reach the game
	CHECK(f1Taken);
	CHECK_FALSE(keyTakenWhileHidden); // a hidden editor leaves every other event to the game
}

TEST_CASE("An editor that was never attached ignores events and wants no pointer", "[editor]")
{
	engine::editor::Editor editor;
	editor.SetVisible(true);
	SDL_Event f1{};
	f1.type = SDL_EVENT_KEY_DOWN;
	f1.key.scancode = SDL_SCANCODE_F1;
	CHECK_FALSE(editor.OnEvent(&f1));
	CHECK_FALSE(editor.WantsSystemCursor());
}

TEST_CASE("The hierarchy and inspector panels draw a world without changing it", "[editor][application]")
{
	engine::ecs::World world;
	engine::ecs::RegisterEngineComponents(world.resources.Emplace<engine::ecs::TypeRegistry>());
	const engine::ecs::Entity root = engine::editor::CreateEntity(world.registry, "root");
	engine::editor::CreateEntity(world.registry, "child", root);
	world.registry.Add<engine::ecs::Parent>(
	    root, engine::ecs::Parent{}); // a component with an entity field (still null here)

	Application app(TestConfig());
	ScriptedGame game;
	game.pressF1 = false;
	engine::editor::Editor editor;
	// two entities that are each other's parent, as a bad scene could hold: drawn once, not forever
	const engine::ecs::Entity loopA = world.registry.Create();
	const engine::ecs::Entity loopB = world.registry.Create();
	world.registry.Add<engine::ecs::Parent>(loopA, engine::ecs::Parent{loopB});
	world.registry.Add<engine::ecs::Parent>(loopB, engine::ecs::Parent{loopA});

	const auto panels = engine::editor::AddEntityPanels(editor, world);
	panels->selected = root; // so the inspector draws its fields too
	editor.SetVisible(true);
	app.SetEditorHost(&editor);
	if (app.Run(game) != 0) SKIP("no offscreen video driver");

	CHECK(game.renders > 0);
	CHECK(world.registry.EntityCount() == 4);
}

namespace
{
	struct Picture
	{
		std::string clip = "button";
	};
} // namespace

TEST_CASE("The inspector draws a text field with choices as a list, and a value outside it", "[editor][application]")
{
	engine::ecs::World world;
	auto& types = world.resources.Emplace<engine::ecs::TypeRegistry>();
	types.Register<Picture>("Picture").Field("clip", &Picture::clip, {.choices = {"panel", "button"}});
	const engine::ecs::TypeInfo* picture = types.Find("Picture");
	REQUIRE(picture != nullptr);
	const auto& pictureFields = picture->Fields();
	REQUIRE(pictureFields.size() == 1);
	const std::size_t choiceCount = pictureFields.front().hints.choices.size();
	CHECK(choiceCount == 2);
	const engine::ecs::Entity one = engine::editor::CreateEntity(world.registry, "one");
	const engine::ecs::Entity two = engine::editor::CreateEntity(world.registry, "two");
	world.registry.Add<Picture>(one, Picture{"panel"});
	world.registry.Add<Picture>(two, Picture{"gone"}); // not in the list

	Application app(TestConfig());
	ScriptedGame game;
	game.pressF1 = false;
	engine::editor::Editor editor;
	const auto panels = engine::editor::AddEntityPanels(editor, world);
	game.onUpdate = [&](int update)
	{
		panels->selected = update % 2 == 0 ? one : two; // both are drawn over the run
	};
	editor.SetVisible(true);
	app.SetEditorHost(&editor);
	if (app.Run(game) != 0) SKIP("no offscreen video driver");

	CHECK(game.renders > 0);
	CHECK(world.registry.GetComponent<Picture>(one).clip == "panel");
	CHECK(world.registry.GetComponent<Picture>(two).clip == "gone"); // drawing never changes it
}

TEST_CASE("The game gets arrow keys while the editor says it has the keyboard", "[editor][application]")
{
	Application app(TestConfig());
	ScriptedGame game;
	game.pressF1 = false;
	engine::editor::Editor editor;
	bool takenByDefault = false;
	bool takenWithGameKeys = true;
	game.onUpdate = [&](int update)
	{
		if (update != 3) return;
		editor.SetVisible(true);
		SDL_Event arrow{};
		arrow.type = SDL_EVENT_KEY_DOWN;
		arrow.key.scancode = SDL_SCANCODE_LEFT;
		// With keyboard navigation active ImGui wants the key; the first press switches it on.
		takenByDefault = editor.OnEvent(&arrow);
		editor.SetGameKeys(true);
		takenWithGameKeys = editor.OnEvent(&arrow);
		editor.SetGameKeys(false);
		editor.SetVisible(false);
	};
	app.SetEditorHost(&editor);
	if (app.Run(game) != 0) SKIP("no offscreen video driver");

	(void)takenByDefault; // depends on ImGui's navigation state, so it is not asserted
	CHECK_FALSE(takenWithGameKeys);
	CHECK_FALSE(editor.GameKeys());
}
