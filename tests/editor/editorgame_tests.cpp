#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "application/scheduledgame.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/name.hpp"
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

	class TestGame final : public ScheduledGame
	{
	public:
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
		world.registry.Add<Transform>(entity, Transform{{x, 0}});
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
