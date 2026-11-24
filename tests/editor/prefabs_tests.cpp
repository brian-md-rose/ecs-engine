#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "ecs/enginecomponents.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/name.hpp"
#include "ecs/prefabinstance.hpp"
#include "ecs/registry.hpp"
#include "ecs/typeregistry.hpp"
#include "editor/entityops.hpp"
#include "editor/prefabs.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::Name;
	using engine::ecs::Parent;
	using engine::ecs::PrefabInstance;
	using engine::ecs::Registry;
	using engine::editor::CreateEntity;
	using engine::graphics::Transform;

	// A scratch folder that is removed afterwards.
	struct ScratchFolder
	{
		std::filesystem::path path;
		ScratchFolder()
		    : path(std::filesystem::temp_directory_path() / ("ecs-engine-prefab-test-" + std::to_string(Counter())))
		{
			std::filesystem::remove_all(path);
		}
		~ScratchFolder()
		{
			std::error_code error;
			std::filesystem::remove_all(path, error);
		}
		static int& Counter()
		{
			static int next = 0;
			return ++next;
		}
	};

	// A game component that points at another entity.
	struct Target
	{
		Entity who = Entity::Null;
	};

	engine::ecs::TypeRegistry MakeTypes()
	{
		engine::ecs::TypeRegistry types;
		engine::ecs::RegisterEngineComponents(types);
		types.Register<Target>("Target").Field("who", &Target::who);
		return types;
	}

	// A door: a frame with a handle under it.
	Entity MakeDoor(Registry& registry, float x)
	{
		const Entity door = CreateEntity(registry, "Door");
		registry.Add<Transform>(door, Transform{.position = {x, 5}});
		const Entity handle = CreateEntity(registry, "Handle", door);
		registry.Add<Transform>(handle, Transform{});
		registry.Add<engine::graphics::LocalTransform>(handle, engine::graphics::LocalTransform{.position = {1, 2}});
		return door;
	}
} // namespace

TEST_CASE("Prefab names are plain file names", "[editor][prefab]")
{
	using engine::editor::IsValidPrefabName;
	CHECK(IsValidPrefabName("door"));
	CHECK(IsValidPrefabName("Big Door 2"));
	CHECK_FALSE(IsValidPrefabName(""));
	CHECK_FALSE(IsValidPrefabName("../door"));
	CHECK_FALSE(IsValidPrefabName("a/b"));
	CHECK_FALSE(IsValidPrefabName("door.json"));
	CHECK_FALSE(IsValidPrefabName("C:door"));
	CHECK_FALSE(IsValidPrefabName("what?"));
	CHECK_FALSE(IsValidPrefabName("a|b"));
	CHECK_FALSE(IsValidPrefabName("door "));
	CHECK_FALSE(IsValidPrefabName("tab\there"));
}

TEST_CASE("A saved prefab can be put into a scene as a copy of the whole tree", "[editor][prefab]")
{
	ScratchFolder scratch;
	const auto types = MakeTypes();
	Registry registry;
	const Entity door = MakeDoor(registry, 10);

	std::string error;
	REQUIRE(engine::editor::SavePrefab(registry, types, door, scratch.path, "door", error));
	CHECK(error.empty());
	CHECK(engine::editor::PrefabNameOf(registry, door) == "door");
	CHECK(engine::editor::ListPrefabs(scratch.path) == std::vector<std::string>{"door"});

	const Entity copy = engine::editor::InstantiatePrefab(registry, types, scratch.path, "door", error);
	REQUIRE_FALSE(copy.IsNull());
	CHECK(copy != door);
	CHECK(registry.TryGetComponent<Name>(copy)->value == "Door");
	CHECK(engine::editor::PrefabNameOf(registry, copy) == "door");
	CHECK(registry.TryGetComponent<Transform>(copy)->position.x == 10.0F);
	const auto below = engine::ecs::DescendantsOf(registry, copy);
	REQUIRE(below.size() == 1);
	CHECK(registry.TryGetComponent<Name>(below.front())->value == "Handle");
	CHECK(registry.TryGetComponent<Parent>(below.front())->entity == copy); // points at the copy, not the original
	CHECK(registry.TryGetComponent<engine::graphics::LocalTransform>(below.front())->position.y == 2.0F);
}

TEST_CASE("Saving a prefab refuses a bad name and a tree that refers outside itself", "[editor][prefab]")
{
	ScratchFolder scratch;
	const auto types = MakeTypes();
	Registry registry;
	const Entity door = MakeDoor(registry, 0);
	std::string error;

	CHECK_FALSE(engine::editor::SavePrefab(registry, types, door, scratch.path, "../evil", error));
	CHECK_FALSE(error.empty());
	CHECK_FALSE(registry.HasComponent<PrefabInstance>(door)); // left alone
	CHECK(engine::editor::ListPrefabs(scratch.path).empty());

	// The handle's Parent points at the door, which is in the tree; saving only the handle puts it outside.
	const Entity handle = engine::ecs::DescendantsOf(registry, door).front();
	CHECK(engine::editor::SavePrefab(registry, types, handle, scratch.path, "handle", error)); // its Parent is dropped
	CHECK_FALSE(engine::editor::SavePrefab(registry, types, Entity::Null, scratch.path, "none", error));

	// A field that points at an entity outside the tree can't be saved, and the root is left as it was.
	const Entity elsewhere = CreateEntity(registry, "Elsewhere");
	registry.Add<Target>(handle, Target{elsewhere});
	error.clear();
	CHECK_FALSE(engine::editor::SavePrefab(registry, types, door, scratch.path, "pointer", error));
	CHECK_FALSE(error.empty());
	CHECK_FALSE(registry.HasComponent<PrefabInstance>(door));
	CHECK(engine::editor::ListPrefabs(scratch.path) == std::vector<std::string>{"handle"});
}

TEST_CASE("A prefab that can't be written leaves the copy's PrefabInstance and the old file as they were",
          "[editor][prefab]")
{
	ScratchFolder scratch;
	const auto types = MakeTypes();
	Registry registry;
	const Entity door = MakeDoor(registry, 0);
	std::string error;
	REQUIRE(engine::editor::SavePrefab(registry, types, door, scratch.path, "door", error));
	const std::string before = [&]
	{
		std::ifstream in(scratch.path / "door.json", std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(in), {});
	}();

	// A folder that is really a file: nothing can be written there.
	const std::filesystem::path blocked = scratch.path / "blocked";
	std::ofstream(blocked) << "not a folder";
	registry.TryGetComponent<Name>(door)->value = "Changed";
	CHECK_FALSE(engine::editor::SavePrefab(registry, types, door, blocked, "other", error));
	CHECK_FALSE(error.empty());
	CHECK(engine::editor::PrefabNameOf(registry, door) == "door"); // not "other"

	// And a new root with no PrefabInstance gets none.
	const Entity fresh = MakeDoor(registry, 3);
	CHECK_FALSE(engine::editor::SavePrefab(registry, types, fresh, blocked, "other", error));
	CHECK_FALSE(registry.HasComponent<PrefabInstance>(fresh));

	std::ifstream in(scratch.path / "door.json", std::ios::binary);
	CHECK(std::string(std::istreambuf_iterator<char>(in), {}) == before); // the old file is intact
}

TEST_CASE("Instantiating a prefab that is missing or broken changes nothing", "[editor][prefab]")
{
	ScratchFolder scratch;
	const auto types = MakeTypes();
	Registry registry;
	std::string error;
	const auto before = registry.AllEntities().size();

	CHECK(engine::editor::InstantiatePrefab(registry, types, scratch.path, "nothing", error).IsNull());
	CHECK_FALSE(error.empty());

	std::filesystem::create_directories(scratch.path);
	std::ofstream(scratch.path / "bad.json") << "{ not json";
	error.clear();
	CHECK(engine::editor::InstantiatePrefab(registry, types, scratch.path, "bad", error).IsNull());
	CHECK_FALSE(error.empty());
	CHECK(registry.AllEntities().size() == before);
}

TEST_CASE("Apply writes a copy's changes into the prefab, and Revert undoes changes made to a copy", "[editor][prefab]")
{
	ScratchFolder scratch;
	const auto types = MakeTypes();
	Registry registry;
	const Entity door = MakeDoor(registry, 10);
	std::string error;
	REQUIRE(engine::editor::SavePrefab(registry, types, door, scratch.path, "door", error));

	// A copy, changed: renamed, and a child added.
	const Entity copy = engine::editor::InstantiatePrefab(registry, types, scratch.path, "door", error);
	REQUIRE_FALSE(copy.IsNull());
	registry.TryGetComponent<Name>(copy)->value = "Gate";
	(void)CreateEntity(registry, "Lock", copy);

	// Revert brings back the prefab's name and children, in the copy's place and at its position.
	registry.TryGetComponent<Transform>(copy)->position = {40, 50};
	const Entity reverted = engine::editor::RevertToPrefab(registry, types, copy, scratch.path, error);
	REQUIRE_FALSE(reverted.IsNull());
	CHECK_FALSE(registry.IsAlive(copy));
	CHECK(registry.TryGetComponent<Name>(reverted)->value == "Door");
	CHECK(engine::ecs::DescendantsOf(registry, reverted).size() == 1);
	CHECK(registry.TryGetComponent<Transform>(reverted)->position.x == 40.0F);
	CHECK(registry.TryGetComponent<Transform>(reverted)->position.y == 50.0F);

	// Apply: change the copy and write it back; a later copy has the change.
	registry.TryGetComponent<Name>(reverted)->value = "Gate";
	REQUIRE(engine::editor::ApplyToPrefab(registry, types, reverted, scratch.path, error));
	const Entity again = engine::editor::InstantiatePrefab(registry, types, scratch.path, "door", error);
	REQUIRE_FALSE(again.IsNull());
	CHECK(registry.TryGetComponent<Name>(again)->value == "Gate");

	// Neither works on something that is not an instance.
	const Entity plain = CreateEntity(registry, "Plain");
	CHECK_FALSE(engine::editor::ApplyToPrefab(registry, types, plain, scratch.path, error));
	CHECK(engine::editor::RevertToPrefab(registry, types, plain, scratch.path, error).IsNull());
}

TEST_CASE("Revert keeps the copy's parent, and keeps the scene as it was if the prefab is gone", "[editor][prefab]")
{
	ScratchFolder scratch;
	const auto types = MakeTypes();
	Registry registry;
	std::string error;
	const Entity door = MakeDoor(registry, 0);
	REQUIRE(engine::editor::SavePrefab(registry, types, door, scratch.path, "door", error));

	const Entity room = CreateEntity(registry, "Room");
	Entity copy = engine::editor::InstantiatePrefab(registry, types, scratch.path, "door", error);
	REQUIRE(engine::editor::SetParent(registry, copy, room));
	copy = engine::editor::RevertToPrefab(registry, types, copy, scratch.path, error);
	REQUIRE_FALSE(copy.IsNull());
	REQUIRE(registry.HasComponent<Parent>(copy));
	CHECK(registry.TryGetComponent<Parent>(copy)->entity == room);

	std::filesystem::remove(scratch.path / "door.json");
	CHECK(engine::editor::RevertToPrefab(registry, types, copy, scratch.path, error).IsNull());
	CHECK(registry.IsAlive(copy)); // still there
}
