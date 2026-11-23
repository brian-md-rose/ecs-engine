#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "core/version.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/name.hpp"
#include "ecs/registry.hpp"
#include "ecs/scene.hpp"
#include "ecs/typeregistry.hpp"
#include "graphics/interpolation.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"

namespace
{
	using engine::core::JsonValue;
	using engine::ecs::Entity;
	using engine::ecs::Issues;
	using engine::ecs::LoadScene;
	using engine::ecs::LoadSceneFile;
	using engine::ecs::Name;
	using engine::ecs::Parent;
	using engine::ecs::Registry;
	using engine::ecs::SaveScene;
	using engine::ecs::SaveSceneFile;
	using engine::ecs::TypeRegistry;
	using namespace engine::graphics;

	struct Follow
	{
		Entity target = Entity::Null;
		float speed = 1.0f;
	};

	struct Cache
	{
		int value = 0;
	};

	TypeRegistry MakeTypes()
	{
		TypeRegistry types;
		engine::ecs::RegisterEngineComponents(types);
		types.Register<Follow>("Follow").Field("target", &Follow::target).Field("speed", &Follow::speed);
		return types;
	}

	JsonValue Parse(const char* text)
	{
		auto result = engine::core::ParseJson(text);
		REQUIRE(result.value.has_value());
		return *result.value;
	}

	std::string Compact(const JsonValue& value)
	{
		return engine::core::FormatJson(value, 0);
	}
} // namespace

TEST_CASE("Registry::AllEntities lists the live entities in order of id", "[ecs][scene]")
{
	Registry registry;
	const Entity a = registry.Create();
	const Entity b = registry.Create();
	const Entity c = registry.Create();
	registry.Destroy(b);
	CHECK(registry.AllEntities() == std::vector<Entity>{a, c});
}

TEST_CASE("A scene is written in a stable, readable shape", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	Registry registry;
	const Entity player = registry.Create();
	registry.Add<Name>(player, Name{"Player"});
	registry.Add<Transform>(player, Transform{.position = {16.0f, 24.5f}});
	registry.Add<Cache>(player);         // not registered: left out
	registry.Add<Interpolation>(player); // Runtime: left out
	const Entity label = registry.Create();
	registry.Add<Parent>(label, Parent{player});
	registry.Add<Follow>(label, Follow{player, 0.5f});

	Issues issues;
	const JsonValue scene = SaveScene(registry, types, issues);
	CHECK(issues.Ok());
	const std::string expected =
	    std::string(R"({"format":1,"engine":")") + engine::kVersionString +
	    R"(","entities":[{"id":1,"name":"Player","components":{"Transform":{"position":{"x":16.0,"y":24.5},"rotation":0.0,"scale":{"x":1.0,"y":1.0}}}},)" +
	    R"({"id":2,"parent":1,"components":{"Follow":{"target":1,"speed":0.5}}}]})";
	CHECK(Compact(scene) == expected);
	CHECK(Compact(SaveScene(registry, types, issues)) == expected); // the same twice
}

TEST_CASE("A scene round-trips through save and load", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	Registry source;
	const Entity a = source.Create();
	const Entity b = source.Create();
	source.Add<Name>(a, Name{"A"});
	source.Add<Shape>(a, Shape{ShapeKind::Circle, 0, 0, 3.0f, Color{1, 2, 3}, 4});
	source.Add<Follow>(a, Follow{b, 2.0f});
	source.Add<Parent>(b, Parent{a});
	source.Add<Transform>(b, Transform{.position = {1.0f, 2.0f}});

	Issues saveIssues;
	const JsonValue scene = SaveScene(source, types, saveIssues);
	REQUIRE(saveIssues.Ok());

	Registry loaded;
	const auto result = LoadScene(loaded, types, scene);
	REQUIRE(result.Ok());
	REQUIRE(result.entities.size() == 2);
	const Entity a2 = result.entities[0];
	const Entity b2 = result.entities[1];
	CHECK(loaded.GetComponent<Name>(a2).value == "A");
	CHECK(loaded.GetComponent<Shape>(a2).radius == 3.0f);
	CHECK(loaded.GetComponent<Follow>(a2).target == b2); // remapped to the new entity
	CHECK(loaded.GetComponent<Parent>(b2).entity == a2);
	CHECK(loaded.GetComponent<Transform>(b2).position.y == 2.0f);

	Issues again;
	CHECK(SaveScene(loaded, types, again) == scene);
}

TEST_CASE("References may point forwards and entity order does not matter", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	Registry registry;
	const auto result = LoadScene(registry, types, Parse(R"({"format": 1, "entities": [
	    {"id": 7, "parent": 3, "components": {"Follow": {"target": 3}}},
	    {"id": 3, "name": "Root"}]})"));
	REQUIRE(result.Ok());
	CHECK(registry.GetComponent<Parent>(result.entities[0]).entity == result.entities[1]);
	CHECK(registry.GetComponent<Follow>(result.entities[0]).target == result.entities[1]);
}

TEST_CASE("Loading adds components through Add, so OnAdd hooks run", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	Registry registry;
	int added = 0;
	registry.OnAdd<Transform>([&added](Entity, Transform&)
	{
		++added;
	});
	REQUIRE(LoadScene(registry, types, Parse(R"({"format": 1, "entities": [
	    {"id": 1, "components": {"Transform": {}}}, {"id": 2, "components": {"Transform": {}}}]})"))
	            .Ok());
	CHECK(added == 2);
}

TEST_CASE("An unknown component is a warning and the scene still loads", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	Registry registry;
	const auto result = LoadScene(registry, types, Parse(R"({"format": 1, "entities": [
	    {"id": 1, "components": {"Removed": {"a": 1}, "Interpolation": {}, "Transform": {}}}]})"));
	REQUIRE(result.Ok());
	REQUIRE(result.issues.warnings.size() == 2);
	CHECK(result.issues.warnings[0].path == "entities[0].components.Removed");
	CHECK(registry.HasComponent<Transform>(result.entities[0]));
	CHECK_FALSE(registry.HasComponent<Interpolation>(result.entities[0]));
}

TEST_CASE("Load errors carry JSON paths and undo the whole load", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	Registry registry;
	const Entity existing = registry.Create();
	const auto result = LoadScene(registry, types, Parse(R"({"format": 1, "entities": [
	    {"id": 1},
	    {"id": 2},
	    {"id": 2},
	    {"id": 4, "parent": 99, "components": {"Follow": {"target": 55, "speed": "fast"}}}]})"));
	CHECK_FALSE(result.Ok());
	CHECK(result.entities.empty());
	std::vector<std::string> paths;
	for (const auto& error : result.issues.errors)
	{
		paths.push_back(error.path);
	}
	CHECK(paths == std::vector<std::string>{"entities[2].id", "entities[3].parent",
	                                        "entities[3].components.Follow.target",
	                                        "entities[3].components.Follow.speed"});
	CHECK(registry.EntityCount() == 1);
	CHECK(registry.IsAlive(existing));
}

TEST_CASE("A scene with a bad header is rejected", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	Registry registry;
	CHECK_FALSE(LoadScene(registry, types, Parse("[]")).Ok());
	CHECK_FALSE(LoadScene(registry, types, Parse(R"({"entities": []})")).Ok());
	CHECK_FALSE(LoadScene(registry, types, Parse(R"({"format": 2, "entities": []})")).Ok());
	CHECK_FALSE(LoadScene(registry, types, Parse(R"({"format": 1})")).Ok());
	CHECK(LoadScene(registry, types, Parse(R"({"format": 1, "entities": []})")).Ok());
	CHECK(registry.EntityCount() == 0);
}

TEST_CASE("Saving a selection errors on references that leave it", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	Registry registry;
	const Entity a = registry.Create();
	const Entity b = registry.Create();
	registry.Add<Follow>(a, Follow{b, 1.0f});
	registry.Add<Parent>(a, Parent{b});

	Issues issues;
	const std::vector<Entity> selection = {a};
	(void)SaveScene(registry, types, issues, selection);
	REQUIRE(issues.errors.size() == 2);
	CHECK(issues.errors[0].path == "entities[0].parent");
	CHECK(issues.errors[1].path == "entities[0].components.Follow.target");
}

TEST_CASE("A scene file round-trips and a missing file is an error", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	const auto dir = std::filesystem::temp_directory_path() / "ecs-engine-scene-tests";
	std::filesystem::create_directories(dir);
	const auto file = dir / "scene.json";

	Registry source;
	const Entity e = source.Create();
	source.Add<Name>(e, Name{"Saved"});
	source.Add<Transform>(e, Transform{.position = {3.0f, 4.0f}});
	Issues issues;
	REQUIRE(SaveSceneFile(source, types, file, issues));

	Registry loaded;
	const auto result = LoadSceneFile(loaded, types, file);
	REQUIRE(result.Ok());
	REQUIRE(result.entities.size() == 1);
	CHECK(loaded.GetComponent<Name>(result.entities[0]).value == "Saved");

	Registry other;
	CHECK_FALSE(LoadSceneFile(other, types, dir / "missing.json").Ok());
	std::filesystem::remove_all(dir);
}

TEST_CASE("A parent loop is a load error", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	Registry registry;
	const auto result =
	    LoadScene(registry, types, Parse(R"({"format": 1, "entities": [{"id": 1, "parent": 2}, {"id": 2, "parent": 1},
	    {"id": 3, "parent": 3}, {"id": 4, "parent": 1}]})"));
	CHECK_FALSE(result.Ok());
	CHECK(registry.EntityCount() == 0);
	CHECK(result.issues.errors.size() >= 3);
}

TEST_CASE("SaveSceneFile ignores errors the caller already had", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	const auto dir = std::filesystem::temp_directory_path() / "ecs-engine-scene-tests-2";
	std::filesystem::create_directories(dir);
	Registry registry;
	Issues issues;
	issues.errors.push_back({"old", "from before"});
	CHECK(SaveSceneFile(registry, types, dir / "empty.json", issues));
	std::filesystem::remove_all(dir);
}

TEST_CASE("Scene settings round-trip through a scene file and are optional", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	const auto dir = std::filesystem::temp_directory_path() / "ecs-engine-scene-settings-tests";
	std::filesystem::create_directories(dir);
	const auto file = dir / "scene.json";
	Registry source;
	static_cast<void>(source.Create());

	engine::ecs::SceneSettings settings;
	settings.camera = engine::ecs::SceneCamera{12.5F, -3.0F, 2};
	settings.background = std::array<std::uint8_t, 3>{10, 20, 30};
	Issues issues;
	REQUIRE(SaveSceneFile(source, types, file, issues, &settings));

	Registry loaded;
	engine::ecs::SceneSettings back;
	const auto result = LoadSceneFile(loaded, types, file, &back);
	REQUIRE(result.Ok());
	CHECK(back == settings);

	// A file written without settings reads as empty, and saving empty settings leaves the entry out.
	const engine::ecs::SceneSettings none;
	REQUIRE(SaveSceneFile(source, types, file, issues, &none));
	Registry again;
	back = settings;
	REQUIRE(LoadSceneFile(again, types, file, &back).Ok());
	CHECK(back.Empty());
	std::filesystem::remove_all(dir);
}

TEST_CASE("Bad scene settings are left out with a warning", "[ecs][scene]")
{
	const auto parse = [](const char* text)
	{
		const auto parsed = engine::core::ParseJson(text);
		REQUIRE(parsed.value.has_value());
		return *parsed.value;
	};
	Issues issues;
	CHECK(engine::ecs::ReadSceneSettings(parse(R"({"entities": []})"), issues).Empty());
	CHECK(issues.warnings.empty());

	const auto bad = engine::ecs::ReadSceneSettings(
	    parse(R"({"settings": {"camera": {"x": 1}, "background": [1, 2, 300]}})"), issues);
	CHECK(bad.Empty());
	CHECK(issues.warnings.size() == 2);

	const auto good = engine::ecs::ReadSceneSettings(parse(R"({"settings": {"camera": {"x": 1, "y": 2}}})"), issues);
	REQUIRE(good.camera.has_value());
	CHECK(good.camera->zoom == 1); // zoom may be left out
	CHECK_FALSE(good.background.has_value());

	engine::core::JsonValue scene = parse(R"({"format": 1, "entities": [], "settings": {"background": [1, 2, 3]}})");
	engine::ecs::WriteSceneSettings(scene, engine::ecs::SceneSettings{});
	CHECK(scene.Find("settings") == nullptr);
	CHECK(scene.Find("entities") != nullptr);
}

TEST_CASE("A scene written before rotation and scale existed loads with the defaults", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	const JsonValue scene =
	    Parse(R"({"format":1,"entities":[{"id":1,"components":{"Transform":{"position":{"x":3,"y":4}},)"
	          R"("LocalTransform":{"position":{"x":1,"y":2}}}}]})");

	Registry loaded;
	const auto result = LoadScene(loaded, types, scene);
	REQUIRE(result.Ok());
	REQUIRE(result.entities.size() == 1);
	const Transform& transform = loaded.GetComponent<Transform>(result.entities[0]);
	CHECK(transform.position.x == 3.0f);
	CHECK(transform.rotation == 0.0f);
	CHECK(transform.scale.x == 1.0f);
	CHECK(transform.scale.y == 1.0f);
	const auto& local = loaded.GetComponent<engine::graphics::LocalTransform>(result.entities[0]);
	CHECK(local.scale.x == 1.0f);
}

TEST_CASE("Rotation and scale round-trip through a scene", "[ecs][scene]")
{
	const TypeRegistry types = MakeTypes();
	Registry source;
	const Entity a = source.Create();
	source.Add<Transform>(a, Transform{.position = {1.0f, 2.0f}, .rotation = 45.0f, .scale = {2.0f, -0.5f}});

	Issues issues;
	const JsonValue scene = SaveScene(source, types, issues);
	REQUIRE(issues.Ok());
	Registry loaded;
	const auto result = LoadScene(loaded, types, scene);
	REQUIRE(result.Ok());
	const Transform& transform = loaded.GetComponent<Transform>(result.entities[0]);
	CHECK(transform.rotation == 45.0f);
	CHECK(transform.scale.x == 2.0f);
	CHECK(transform.scale.y == -0.5f);
}
