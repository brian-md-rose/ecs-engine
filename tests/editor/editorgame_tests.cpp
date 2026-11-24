#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "application/scheduledgame.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/name.hpp"
#include "ecs/scene.hpp"
#include "ecs/typeregistry.hpp"
#include "editor/editor.hpp"
#include "editor/editorgame.hpp"
#include "editor/entityops.hpp"
#include "graphics/transform.hpp"
#include "input/input.hpp"

namespace
{
	using engine::application::ScheduledGame;
	using engine::ecs::Entity;
	using engine::ecs::Name;
	using engine::editor::EditorGame;
	using engine::editor::EditorGameConfig;
	using engine::graphics::Transform;

	struct Mover
	{
		float speed = 10;
	};

	void MoveSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context)
	{
		world.registry.Each<Transform, Mover>([&](Entity, Transform& transform, Mover& mover)
		{
			transform.position.x += mover.speed * static_cast<float>(context.dt);
		});
	}

	bool gTestQuit = false; // makes every TestGame ask to quit

	class TestGame final : public ScheduledGame
	{
	public:
		[[nodiscard]] bool WantsToQuit() const override { return gTestQuit; }

		TestGame()
		{
			auto& types = world.resources.Emplace<engine::ecs::TypeRegistry>();
			engine::ecs::RegisterEngineComponents(types);
			types.Register<Mover>("Mover").Field("speed", &Mover::speed);
			scheduler.Add(engine::ecs::Phase::Update, MoveSystem);
		}
	};

	std::filesystem::path TempFolder(const char* name)
	{
		const auto folder = std::filesystem::temp_directory_path() / "ecs_engine_editorgame" / name;
		std::filesystem::remove_all(folder);
		return folder;
	}

	EditorGameConfig MakeConfig(const std::filesystem::path& folder)
	{
		return {.makeGame =
		            []
		{
			return std::make_unique<TestGame>();
		},
		        .afterLoad = {},
		        .sceneFolder = folder};
	}

	Entity AddMover(engine::ecs::World& world, const std::string& name, float x)
	{
		const Entity entity = engine::editor::CreateEntity(world.registry, name);
		world.registry.Add<Transform>(entity, Transform{.position = {x, 0}});
		world.registry.Add<Mover>(entity);
		return entity;
	}
} // namespace

TEST_CASE("A new scene starts empty, and changes make it dirty", "[editor][game]")
{
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(TempFolder("dirty")));
	REQUIRE(game.IsValid());
	CHECK(game.EditWorld().registry.AllEntities().empty());
	CHECK_FALSE(game.IsDirty());

	AddMover(game.EditWorld(), "a", 1);
	game.CheckDirty();
	CHECK(game.IsDirty());
}

TEST_CASE("Save as writes a file that Open reads back", "[editor][game]")
{
	const auto folder = TempFolder("saveopen");
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(folder));
	REQUIRE(game.IsValid());

	AddMover(game.EditWorld(), "a", 5);
	CHECK_FALSE(game.SaveScene()); // no file yet
	CHECK(game.SaveSceneAs("level"));
	CHECK(game.SceneName() == "level.json");
	CHECK(std::filesystem::exists(folder / "level.json"));
	game.CheckDirty();
	CHECK_FALSE(game.IsDirty());

	game.NewScene();
	CHECK(game.EditWorld().registry.AllEntities().empty());
	CHECK(game.OpenScene("level.json"));
	REQUIRE(game.EditWorld().registry.AllEntities().size() == 1);
	CHECK_FALSE(game.IsDirty());

	// A bad name or a missing file leaves the open scene alone.
	CHECK_FALSE(game.SaveSceneAs("../escape"));
	CHECK_FALSE(game.OpenScene("missing.json"));
	CHECK_FALSE(game.Message().empty());
	CHECK(game.EditWorld().registry.AllEntities().size() == 1);
}

TEST_CASE("Play runs a copy and Stop leaves the edit world as it was", "[editor][game]")
{
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(TempFolder("play")));
	REQUIRE(game.IsValid());
	const Entity edited = AddMover(game.EditWorld(), "a", 0);

	REQUIRE(game.StartPlay());
	CHECK(game.IsPlaying());
	engine::input::Input input;
	for (int i = 0; i < 10; ++i)
		game.Update(0.1, input, nullptr);

	REQUIRE(game.PlayWorld() != nullptr);
	const auto playIds = game.PlayWorld()->registry.AllEntities();
	REQUIRE(playIds.size() == 1);
	CHECK(game.PlayWorld()->registry.TryGetComponent<Transform>(playIds[0])->position.x > 5.0F);
	CHECK(game.EditWorld().registry.TryGetComponent<Transform>(edited)->position.x == 0.0F);

	game.StopPlay();
	CHECK_FALSE(game.IsPlaying());
	CHECK(game.EditWorld().registry.TryGetComponent<Transform>(edited)->position.x == 0.0F);
	CHECK(game.EditWorld().registry.TryGetComponent<Name>(edited)->value == "a");
}

TEST_CASE("Edits made before Play still count as unsaved while playing", "[editor][game]")
{
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(TempFolder("playdirty")));
	REQUIRE(game.IsValid());
	AddMover(game.EditWorld(), "a", 0);
	REQUIRE(game.StartPlay());
	CHECK(game.IsDirty());
	game.StopPlay();
}

TEST_CASE("afterLoad runs on Open and Play, and a game that can't be made fails Play", "[editor][game]")
{
	const auto folder = TempFolder("afterload");
	int loads = 0;
	bool failMake = false;
	EditorGameConfig config = MakeConfig(folder);
	config.afterLoad = [&](engine::ecs::World&)
	{
		++loads;
	};
	config.makeGame = [&]() -> std::unique_ptr<ScheduledGame>
	{
		if (failMake) return nullptr;
		return std::make_unique<TestGame>();
	};

	engine::editor::Editor editor;
	EditorGame game(editor, config);
	REQUIRE(game.IsValid());
	AddMover(game.EditWorld(), "a", 0);
	REQUIRE(game.SaveSceneAs("s"));

	loads = 0;
	REQUIRE(game.OpenScene("s.json"));
	CHECK(loads == 1);
	REQUIRE(game.StartPlay());
	CHECK(loads == 2);
	game.StopPlay();

	failMake = true;
	CHECK_FALSE(game.StartPlay());
	CHECK_FALSE(game.IsPlaying());
	CHECK_FALSE(game.Message().empty());
}

TEST_CASE("Undo and redo step back through finished changes", "[editor][game][undo]")
{
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(TempFolder("undo")));
	REQUIRE(game.IsValid());
	CHECK_FALSE(game.CanUndo());

	AddMover(game.EditWorld(), "a", 1);
	game.CheckDirty();
	AddMover(game.EditWorld(), "b", 2);
	game.CheckDirty();
	CHECK(game.CanUndo());
	CHECK(game.EditWorld().registry.AllEntities().size() == 2);

	CHECK(game.Undo());
	CHECK(game.EditWorld().registry.AllEntities().size() == 1);
	CHECK(game.CanRedo());
	CHECK(game.Undo());
	CHECK(game.EditWorld().registry.AllEntities().empty());
	CHECK_FALSE(game.Undo());
	CHECK_FALSE(game.IsDirty());

	CHECK(game.Redo());
	CHECK(game.Redo());
	CHECK(game.EditWorld().registry.AllEntities().size() == 2);
	CHECK_FALSE(game.Redo());

	// A change after an undo ends the redo line.
	CHECK(game.Undo());
	AddMover(game.EditWorld(), "c", 3);
	game.CheckDirty();
	CHECK_FALSE(game.CanRedo());
	CHECK(game.EditWorld().registry.AllEntities().size() == 2);
}

TEST_CASE("Undo restores a moved value and the selection, and not while playing", "[editor][game][undo]")
{
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(TempFolder("undomove")));
	REQUIRE(game.IsValid());
	const Entity entity = AddMover(game.EditWorld(), "a", 5);
	game.CheckDirty();

	game.EditWorld().registry.TryGetComponent<Transform>(entity)->position.x = 50;
	game.CheckDirty();
	REQUIRE(game.Undo());
	const auto ids = game.EditWorld().registry.AllEntities();
	REQUIRE(ids.size() == 1);
	CHECK(game.EditWorld().registry.TryGetComponent<Transform>(ids[0])->position.x == 5.0F);

	REQUIRE(game.StartPlay());
	CHECK_FALSE(game.CanUndo());
	CHECK_FALSE(game.Redo());
	game.StopPlay();
	CHECK(game.CanRedo());
}

TEST_CASE("Undo takes a change not yet looked at, and the selection follows", "[editor][game][undo]")
{
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(TempFolder("undopending")));
	REQUIRE(game.IsValid());
	AddMover(game.EditWorld(), "a", 1);
	game.CheckDirty();
	AddMover(game.EditWorld(), "b", 2);
	// No CheckDirty: Undo must still be able to bring "b" back with Redo.
	REQUIRE(game.Undo());
	CHECK(game.EditWorld().registry.AllEntities().size() == 1);
	REQUIRE(game.Redo());
	CHECK(game.EditWorld().registry.AllEntities().size() == 2);

	// New starts the list again.
	game.NewScene();
	CHECK_FALSE(game.CanUndo());
	CHECK_FALSE(game.CanRedo());
}

TEST_CASE("onPlay is called on the play world after the scene is loaded", "[editor][game]")
{
	engine::editor::Editor editor;
	EditorGameConfig config = MakeConfig(TempFolder("onplay"));
	engine::ecs::World* seen = nullptr;
	std::size_t entitiesThen = 0;
	std::string problem; // what the hook reports
	config.onPlay = [&](engine::ecs::World& world)
	{
		seen = &world;
		entitiesThen = world.registry.AllEntities().size();
		return problem;
	};
	EditorGame game(editor, std::move(config));
	REQUIRE(game.IsValid());
	AddMover(game.EditWorld(), "a", 1);
	AddMover(game.EditWorld(), "b", 2);

	CHECK(seen == nullptr); // not for the edit world
	REQUIRE(game.StartPlay());
	CHECK(seen == game.PlayWorld());
	CHECK(entitiesThen == 2);

	// A problem stops Play and is shown.
	game.StopPlay();
	problem = "the maze has no player start";
	CHECK_FALSE(game.StartPlay());
	CHECK_FALSE(game.IsPlaying());
	CHECK(game.Message().find("no player start") != std::string::npos);
}

TEST_CASE("The last scene is reopened at the next start, newest first", "[editor][game]")
{
	const auto folder = TempFolder("recent");
	EditorGameConfig config = MakeConfig(folder);
	config.recentFile = folder / "state" / "recent.txt";
	{
		engine::editor::Editor editor;
		EditorGame game(editor, config);
		REQUIRE(game.IsValid());
		CHECK(game.SceneName().empty()); // nothing remembered yet
		AddMover(game.EditWorld(), "a", 5);
		REQUIRE(game.SaveSceneAs("first"));
		game.NewScene();
		AddMover(game.EditWorld(), "b", 6);
		AddMover(game.EditWorld(), "c", 7);
		REQUIRE(game.SaveSceneAs("second"));
		REQUIRE(game.OpenScene("first.json"));
	}
	{
		engine::editor::Editor editor;
		EditorGame game(editor, config);
		REQUIRE(game.IsValid());
		CHECK(game.SceneName() == "first.json");
		CHECK(game.EditWorld().registry.AllEntities().size() == 1);
		CHECK_FALSE(game.IsDirty());
	}

	// A remembered scene that is gone leaves a new scene and no complaint.
	std::filesystem::remove(folder / "first.json");
	engine::editor::Editor editor;
	EditorGame game(editor, config);
	REQUIRE(game.IsValid());
	CHECK(game.SceneName().empty());
	CHECK(game.Message().empty());
}

TEST_CASE("openOnStart wins over the last scene, and says why when it can't open", "[editor][game]")
{
	const auto folder = TempFolder("openonstart");
	EditorGameConfig config = MakeConfig(folder);
	config.recentFile = folder / "state" / "recent.txt";
	{
		engine::editor::Editor editor;
		EditorGame game(editor, config);
		REQUIRE(game.IsValid());
		AddMover(game.EditWorld(), "a", 5);
		REQUIRE(game.SaveSceneAs("first"));
		game.NewScene();
		REQUIRE(game.SaveSceneAs("second")); // the last scene
	}
	config.openOnStart = "first"; // no extension needed
	{
		engine::editor::Editor editor;
		EditorGame game(editor, config);
		CHECK(game.SceneName() == "first.json");
		CHECK(game.EditWorld().registry.AllEntities().size() == 1);
	}
	config.openOnStart = "nothing.json";
	engine::editor::Editor editor;
	EditorGame game(editor, config);
	REQUIRE(game.IsValid());
	CHECK(game.SceneName().empty());
	CHECK_FALSE(game.Message().empty());
}

TEST_CASE("The Issues panel lists what the last open reported", "[editor][game]")
{
	const auto folder = TempFolder("issues");
	std::filesystem::create_directories(folder);
	{
		std::ofstream out(folder / "warns.json");
		out << R"({"format":1,"entities":[{"id":1,"name":"a","components":{"NoSuchThing":{}}}]})";
	}
	{
		std::ofstream out(folder / "broken.json");
		out << R"({"format":1,"entities":[{"id":1,"parent":9}]})";
	}
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(folder));
	REQUIRE(game.IsValid());
	CHECK(game.IssueCount() == 0);

	CHECK(game.OpenScene("warns.json")); // an unknown component is a warning, and the scene opens
	CHECK(game.IssueCount() == 1);

	CHECK_FALSE(game.OpenScene("broken.json"));
	CHECK(game.IssueCount() >= 1);
	CHECK(game.SceneName() == "warns.json"); // untouched

	game.NewScene();
	CHECK(game.IssueCount() == 0);

	// A clean save or Play start replaces the list instead of leaving the old entries.
	CHECK(game.OpenScene("warns.json"));
	REQUIRE(game.IssueCount() == 1);
	CHECK(game.SaveSceneAs("clean"));
	CHECK(game.IssueCount() == 0);
	CHECK(game.OpenScene("warns.json"));
	REQUIRE(game.IssueCount() == 1);
	REQUIRE(game.StartPlay());
	CHECK(game.IssueCount() == 0);
}

TEST_CASE("Unsaved changes are autosaved and can be restored", "[editor][game]")
{
	const auto folder = TempFolder("autosave");
	EditorGameConfig config = MakeConfig(folder);
	config.autosaveFolder = folder / "autosave";
	{
		engine::editor::Editor editor;
		EditorGame game(editor, config);
		REQUIRE(game.IsValid());
		AddMover(game.EditWorld(), "a", 5);
		REQUIRE(game.SaveSceneAs("level"));
		CHECK_FALSE(game.WriteAutosave()); // nothing unsaved
		CHECK_FALSE(std::filesystem::exists(config.autosaveFolder / "level.json"));

		AddMover(game.EditWorld(), "b", 6);
		REQUIRE(game.WriteAutosave());
		CHECK(std::filesystem::exists(config.autosaveFolder / "level.json"));

		REQUIRE(game.SaveScene()); // saving removes it
		CHECK_FALSE(std::filesystem::exists(config.autosaveFolder / "level.json"));

		AddMover(game.EditWorld(), "c", 7);
		REQUIRE(game.WriteAutosave()); // the editor "crashes" here, with c unsaved
	}
	// The file on disk has two entities, the autosave three; make the autosave the newer.
	std::filesystem::last_write_time(config.autosaveFolder / "level.json",
	                                 std::filesystem::file_time_type::clock::now() + std::chrono::hours(1));
	{
		engine::editor::Editor editor;
		EditorGame game(editor, config);
		REQUIRE(game.IsValid());
		CHECK(game.OpenScene("level.json"));
		CHECK(game.RecoveryOffered());
		CHECK(game.EditWorld().registry.AllEntities().size() == 2);
		REQUIRE(game.AcceptRecovery());
		CHECK(game.EditWorld().registry.AllEntities().size() == 3);
		CHECK(game.IsDirty()); // still unsaved
	}
	{
		engine::editor::Editor editor;
		EditorGame game(editor, config);
		CHECK(game.OpenScene("level.json"));
		REQUIRE(game.RecoveryOffered());
		game.DeclineRecovery();
		CHECK_FALSE(game.RecoveryOffered());
		CHECK_FALSE(std::filesystem::exists(config.autosaveFolder / "level.json"));
		CHECK(game.EditWorld().registry.AllEntities().size() == 2);
	}
}

TEST_CASE("An autosave older than the scene file is not offered", "[editor][game]")
{
	const auto folder = TempFolder("autosaveold");
	EditorGameConfig config = MakeConfig(folder);
	config.autosaveFolder = folder / "autosave";
	engine::editor::Editor editor;
	EditorGame game(editor, config);
	REQUIRE(game.IsValid());
	AddMover(game.EditWorld(), "a", 5);
	REQUIRE(game.SaveSceneAs("level"));
	AddMover(game.EditWorld(), "b", 6);
	REQUIRE(game.WriteAutosave());
	std::filesystem::last_write_time(config.autosaveFolder / "level.json",
	                                 std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
	game.NewScene(); // drops the autosave
	CHECK_FALSE(std::filesystem::exists(config.autosaveFolder / "level.json"));
	// A stale file left behind by something else is dropped on Open, not offered.
	std::ofstream(config.autosaveFolder / "level.json") << "{}";
	std::filesystem::last_write_time(config.autosaveFolder / "level.json",
	                                 std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
	CHECK(game.OpenScene("level.json"));
	CHECK_FALSE(game.RecoveryOffered());
	CHECK_FALSE(std::filesystem::exists(config.autosaveFolder / "level.json"));
}

TEST_CASE("A restored autosave goes when the scene is clean again, none is written while playing, and a new scene "
          "is recovered too",
          "[editor][game]")
{
	const auto folder = TempFolder("autosavemore");
	EditorGameConfig config = MakeConfig(folder);
	config.autosaveFolder = folder / "autosave";
	{
		engine::editor::Editor editor;
		EditorGame game(editor, config);
		REQUIRE(game.IsValid());
		AddMover(game.EditWorld(), "a", 5); // a new scene that is never saved
		REQUIRE(game.WriteAutosave());
		CHECK(std::filesystem::exists(config.autosaveFolder / "_untitled_.json"));
		REQUIRE(game.StartPlay());
		CHECK_FALSE(game.WriteAutosave()); // not while playing
		game.StopPlay();
	}
	{
		engine::editor::Editor editor;
		EditorGame game(editor, config); // starts on a new scene, finds its autosave
		REQUIRE(game.IsValid());
		REQUIRE(game.RecoveryOffered());
		REQUIRE(game.AcceptRecovery());
		CHECK(game.EditWorld().registry.AllEntities().size() == 1);
		CHECK(game.IsDirty());
		// Taking the entity away again makes the scene match the (empty) saved text.
		for (const Entity entity : game.EditWorld().registry.AllEntities())
			game.EditWorld().registry.Destroy(entity);
		game.CheckDirty();
		CHECK_FALSE(game.IsDirty());
		CHECK_FALSE(std::filesystem::exists(config.autosaveFolder / "_untitled_.json"));
	}
}

TEST_CASE("A game that quits stops Play with a message, and Restart plays again from the edit world", "[editor][game]")
{
	const auto folder = TempFolder("playquit");
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(folder));
	REQUIRE(game.IsValid());
	AddMover(game.EditWorld(), "a", 5);

	CHECK_FALSE(game.RestartPlay()); // not playing
	REQUIRE(game.StartPlay());
	// The play world is a copy: changes made while playing go with Restart.
	for (const Entity entity : game.PlayWorld()->registry.AllEntities())
		game.PlayWorld()->registry.Destroy(entity);
	REQUIRE(game.RestartPlay());
	REQUIRE(game.IsPlaying());
	CHECK(game.PlayWorld()->registry.AllEntities().size() == 1);

	gTestQuit = true;
	game.Update(0.016, engine::input::Input{}, nullptr);
	gTestQuit = false;
	CHECK_FALSE(game.IsPlaying());
	CHECK(game.Message() == "the game quit, so Play stopped");
	CHECK(game.EditWorld().registry.AllEntities().size() == 1);
}

TEST_CASE("Scene settings are part of the scene: dirty, saved, undone and played", "[editor][game]")
{
	const auto folder = TempFolder("settings");
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(folder));
	REQUIRE(game.IsValid());
	const auto settingsOf = [](engine::ecs::World& world) -> engine::ecs::SceneSettings&
	{
		auto* settings = world.resources.TryGet<engine::ecs::SceneSettings>();
		REQUIRE(settings != nullptr);
		return *settings;
	};
	CHECK(settingsOf(game.EditWorld()).Empty());
	CHECK_FALSE(game.IsDirty());

	// Changing only the settings makes the scene dirty, and an undo step.
	settingsOf(game.EditWorld()).background = std::array<std::uint8_t, 3>{1, 2, 3};
	game.CheckDirty();
	CHECK(game.IsDirty());
	settingsOf(game.EditWorld()).camera = engine::ecs::SceneCamera{4.0F, 5.0F, 2};
	game.CheckDirty();
	CHECK(game.Undo());
	CHECK_FALSE(settingsOf(game.EditWorld()).camera.has_value());
	CHECK(settingsOf(game.EditWorld()).background.has_value());
	CHECK(game.Redo());
	CHECK(settingsOf(game.EditWorld()).camera.has_value());

	// Save, start over, open: they come back. A new scene has none.
	CHECK(game.SaveSceneAs("look"));
	const engine::ecs::SceneSettings saved = settingsOf(game.EditWorld());
	game.NewScene();
	CHECK(settingsOf(game.EditWorld()).Empty());
	CHECK(game.OpenScene("look.json"));
	CHECK(settingsOf(game.EditWorld()) == saved);
	CHECK_FALSE(game.IsDirty());

	// The play world gets a copy.
	REQUIRE(game.StartPlay());
	CHECK(settingsOf(*game.PlayWorld()) == saved);
}

TEST_CASE("Play can be paused, stepped and sped up, and a new Play starts running", "[editor][game]")
{
	engine::editor::Editor editor;
	EditorGame game(editor, MakeConfig(TempFolder("playcontrols")));
	REQUIRE(game.IsValid());
	AddMover(game.EditWorld(), "a", 0); // moves 10 units per second
	REQUIRE(game.StartPlay());
	engine::input::Input input;
	const auto positionX = [&]
	{
		const auto ids = game.PlayWorld()->registry.AllEntities();
		REQUIRE(ids.size() == 1);
		return game.PlayWorld()->registry.TryGetComponent<Transform>(ids[0])->position.x;
	};

	game.SetPlayPaused(true);
	game.Update(1.0, input, nullptr);
	CHECK(positionX() == 0.0F); // frozen

	game.StepPlay();
	game.Update(1.0, input, nullptr); // one step is 1/60 s whatever dt is
	CHECK(positionX() == Catch::Approx(10.0F / 60.0F));
	game.Update(1.0, input, nullptr);
	CHECK(positionX() == Catch::Approx(10.0F / 60.0F)); // and only one

	game.SetPlayPaused(false);
	game.SetPlaySpeed(2.0F);
	const float before = positionX();
	game.Update(1.0, input, nullptr);
	CHECK(positionX() - before == Catch::Approx(20.0F));

	game.SetPlaySpeed(100.0F);
	CHECK(game.PlaySpeed() == 8.0F); // kept in range

	game.StepPlay(); // a step while running pauses, as the menu item does
	CHECK(game.PlayPaused());
	game.SetPlayMuted(true);
	CHECK(game.PlayMuted());
	game.SetPlayMuted(false);

	game.SetPlayPaused(true);
	game.StopPlay();
	REQUIRE(game.StartPlay());
	CHECK_FALSE(game.PlayPaused()); // a new Play is not left frozen
	game.StopPlay();
}

TEST_CASE("Play from here calls the game's hook with the cell, Restart repeats it and a plain Play does not",
          "[editor][game]")
{
	std::vector<engine::editor::GridCell> calls;
	std::string refuse;
	EditorGameConfig config = MakeConfig(TempFolder("playfrom"));
	config.onPlayFrom = [&](engine::ecs::World&, engine::editor::GridCell cell)
	{
		calls.push_back(cell);
		return refuse;
	};
	engine::editor::Editor editor;
	EditorGame game(editor, config);
	REQUIRE(game.IsValid());

	REQUIRE(game.StartPlayFrom({3, 4}));
	REQUIRE(calls.size() == 1);
	CHECK(calls[0] == engine::editor::GridCell{3, 4});
	REQUIRE(game.RestartPlay());
	REQUIRE(calls.size() == 2);
	CHECK(calls[1] == engine::editor::GridCell{3, 4});
	game.StopPlay();

	REQUIRE(game.StartPlay()); // a plain Play forgets the cell
	CHECK(calls.size() == 2);
	game.StopPlay();

	refuse = "a wall";
	CHECK_FALSE(game.StartPlayFrom({1, 1}));
	CHECK_FALSE(game.IsPlaying());
	CHECK(game.Message().find("a wall") != std::string::npos);
}

TEST_CASE("A game's own inspectors, commands and menu reach the panels", "[editor][editorgame]")
{
	const auto folder = TempFolder("registrations");
	engine::editor::Editor editor;
	int drawn = 0;
	int ran = 0;
	EditorGameConfig config = MakeConfig(folder);
	config.inspectors["Mover"] = [&](engine::ecs::Registry&, Entity)
	{
		++drawn;
	};
	config.componentCommands.push_back({"Mover", "Stop", [&](engine::ecs::Registry&, Entity)
	{
		++ran;
	}});
	config.entityCommands.push_back({"", "Wave", [&](engine::ecs::Registry&, Entity)
	{
		++ran;
	}});
	config.menu = [](engine::ecs::World&) {};
	EditorGame game(editor, std::move(config));
	REQUIRE(game.IsValid());

	const auto& panels = game.Selection();
	REQUIRE(panels.inspectors.count("Mover") == 1);
	REQUIRE(panels.componentCommands.size() == 1);
	CHECK(panels.componentCommands.front().component == "Mover");
	REQUIRE(panels.entityCommands.size() == 1);

	// What the panels are given runs on the live registry.
	engine::ecs::Registry registry;
	panels.inspectors.at("Mover")(registry, Entity::Null);
	panels.componentCommands.front().run(registry, Entity::Null);
	panels.entityCommands.front().run(registry, Entity::Null);
	CHECK(drawn == 1);
	CHECK(ran == 2);
}
