#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "assert_guard.hpp"
#include "core/json.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/name.hpp"
#include "ecs/registry.hpp"
#include "ecs/typeregistry.hpp"
#include "graphics/clip.hpp"
#include "graphics/interpolation.hpp"
#include "graphics/screenspace.hpp"
#include "graphics/shape.hpp"
#include "graphics/sprite.hpp"
#include "graphics/transform.hpp"

namespace
{
	using engine::core::JsonValue;
	using engine::core::ParseJson;
	using engine::ecs::Entity;
	using engine::ecs::IEntityIds;
	using engine::ecs::Issues;
	using engine::ecs::Registry;
	using engine::ecs::TypeRegistry;
	using namespace engine::graphics;

	// Ids for two entities, 1 and 2, chosen by the test.
	class FakeIds : public IEntityIds
	{
	public:
		std::map<std::int64_t, Entity> byId;

		std::optional<std::int64_t> IdOf(Entity entity) const override
		{
			for (const auto& [id, known] : byId)
			{
				if (known == entity) return id;
			}
			return std::nullopt;
		}
		std::optional<Entity> EntityOf(std::int64_t id) const override
		{
			const auto it = byId.find(id);
			if (it == byId.end()) return std::nullopt;
			return it->second;
		}
	};

	enum class Mood : std::uint8_t
	{
		Calm,
		Angry
	};

	struct Brain
	{
		Mood mood = Mood::Calm;
		int level = 3;
		float speed = 0.1f;
		bool awake = true;
		std::string label = {};
		Entity target = Entity::Null;
		std::vector<Entity> friends = {};
	};

	void RegisterBrain(TypeRegistry& types)
	{
		types.Register<Brain>("Brain")
		    .EnumField("mood", &Brain::mood, {{"Calm", Mood::Calm}, {"Angry", Mood::Angry}})
		    .Field("level", &Brain::level, {.min = 0, .max = 10})
		    .Field("speed", &Brain::speed)
		    .Field("awake", &Brain::awake)
		    .Field("label", &Brain::label)
		    .Field("target", &Brain::target)
		    .Field("friends", &Brain::friends);
	}

	JsonValue Parse(const char* text)
	{
		auto result = ParseJson(text);
		REQUIRE(result.value.has_value());
		return *result.value;
	}
} // namespace

TEST_CASE("Registering a component records its name, fields and hints", "[ecs][typeregistry]")
{
	TypeRegistry types;
	RegisterBrain(types);

	const auto* info = types.Find("Brain");
	REQUIRE(info != nullptr);
	CHECK(types.Find<Brain>() == info);
	CHECK(info->Name() == "Brain");
	REQUIRE(info->Fields().size() == 7);
	CHECK(info->Fields()[0].kind == engine::ecs::FieldKind::Enum);
	CHECK(info->Fields()[0].enumNames == std::vector<std::string>{"Calm", "Angry"});
	CHECK(info->Fields()[1].kind == engine::ecs::FieldKind::Int);
	CHECK(info->Fields()[1].hints.max == 10.0);
	CHECK(info->Fields()[5].kind == engine::ecs::FieldKind::Entity);
	CHECK(info->Fields()[6].kind == engine::ecs::FieldKind::EntityList);
	CHECK(types.Find("Nope") == nullptr);
	CHECK(types.Count() == 1);
}

TEST_CASE("Registering a name or a type twice is a checked error", "[ecs][typeregistry]")
{
	const engine::test::AssertsThrow guard;
	TypeRegistry types;
	RegisterBrain(types);
	CHECK_THROWS_AS(types.Register<Brain>("Other"), engine::test::AssertFailure);
	CHECK_THROWS_AS(types.Register<engine::ecs::Name>("Brain"), engine::test::AssertFailure);
	CHECK_THROWS_AS(types.Register<engine::ecs::Name>("Name")
	                    .Field("value", &engine::ecs::Name::value)
	                    .Field("value", &engine::ecs::Name::value),
	                engine::test::AssertFailure);
}

TEST_CASE("A component round-trips through JSON", "[ecs][typeregistry]")
{
	TypeRegistry types;
	RegisterBrain(types);
	const auto& info = *types.Find("Brain");

	Registry registry;
	const Entity a = registry.Create();
	const Entity b = registry.Create();
	FakeIds ids;
	ids.byId = {{1, a}, {2, b}};
	registry.Add<Brain>(a, Brain{Mood::Angry, 7, 0.1f, false, "boss", b, {a, b}});

	Issues issues;
	JsonValue saved;
	REQUIRE(info.Save(registry, a, ids, saved, issues, "Brain"));
	CHECK(issues.Ok());
	CHECK(engine::core::FormatJson(saved, 0) ==
	      R"({"mood":"Angry","level":7,"speed":0.1,"awake":false,"label":"boss","target":2,"friends":[1,2]})");

	const Entity c = registry.Create();
	REQUIRE(info.Load(registry, c, saved, ids, issues, "Brain"));
	const Brain& loaded = registry.GetComponent<Brain>(c);
	CHECK(loaded.mood == Mood::Angry);
	CHECK(loaded.level == 7);
	CHECK(loaded.speed == 0.1f);
	CHECK(!loaded.awake);
	CHECK(loaded.label == "boss");
	CHECK(loaded.target == b);
	CHECK(loaded.friends == std::vector<Entity>{a, b});
}

TEST_CASE("Loading keeps defaults for missing fields and warns about unknown ones", "[ecs][typeregistry]")
{
	TypeRegistry types;
	RegisterBrain(types);
	Registry registry;
	const Entity e = registry.Create();
	const FakeIds ids;
	Issues issues;

	REQUIRE(types.Find("Brain")->Load(registry, e, Parse(R"({"level": 9, "oldField": 1})"), ids, issues, "p"));
	CHECK(registry.GetComponent<Brain>(e).level == 9);
	CHECK(registry.GetComponent<Brain>(e).speed == 0.1f);
	REQUIRE(issues.warnings.size() == 1);
	CHECK(issues.warnings[0].path == "p.oldField");
}

TEST_CASE("Loading reports every bad field with its path and adds nothing", "[ecs][typeregistry]")
{
	TypeRegistry types;
	RegisterBrain(types);
	Registry registry;
	const Entity e = registry.Create();
	const FakeIds ids;
	Issues issues;

	const auto text = R"({"mood": "Sleepy", "level": "high", "target": 9, "awake": true})";
	CHECK_FALSE(types.Find("Brain")->Load(registry, e, Parse(text), ids, issues, "entities[3].components.Brain"));
	REQUIRE(issues.errors.size() == 3);
	CHECK(issues.errors[0].path == "entities[3].components.Brain.mood");
	CHECK(issues.errors[1].path == "entities[3].components.Brain.level");
	CHECK(issues.errors[2].path == "entities[3].components.Brain.target");
	CHECK(issues.errors[2].message == "no entity with id 9");
	CHECK_FALSE(registry.HasComponent<Brain>(e));
}

TEST_CASE("Integers are range checked and an entity outside the scene can't be saved", "[ecs][typeregistry]")
{
	TypeRegistry types;
	types.Register<Sprite>("Sprite").Field("layer", &Sprite::layer);
	Registry registry;
	const Entity e = registry.Create();
	const FakeIds ids;
	Issues issues;
	CHECK_FALSE(types.Find("Sprite")->Load(registry, e, Parse(R"({"layer": 300})"), ids, issues, "s"));
	CHECK(issues.errors.size() == 1);

	TypeRegistry brains;
	RegisterBrain(brains);
	const Entity other = registry.Create();
	registry.Add<Brain>(e, Brain{.target = other});
	Issues saveIssues;
	JsonValue out;
	CHECK_FALSE(brains.Find("Brain")->Save(registry, e, ids, out, saveIssues, "b"));
	REQUIRE(saveIssues.errors.size() == 1);
	CHECK(saveIssues.errors[0].path == "b.target");
}

TEST_CASE("Tags, Runtime and Add/Remove/Has go through the registry", "[ecs][typeregistry]")
{
	TypeRegistry types;
	engine::ecs::RegisterEngineComponents(types);
	Registry registry;
	const Entity e = registry.Create();

	const auto& tag = *types.Find<ScreenSpace>();
	CHECK(tag.Fields().empty());
	CHECK_FALSE(tag.IsRuntime());
	CHECK_FALSE(tag.Has(registry, e));
	tag.AddDefault(registry, e);
	CHECK(registry.HasComponent<ScreenSpace>(e));
	JsonValue out;
	Issues issues;
	REQUIRE(tag.Save(registry, e, FakeIds{}, out, issues, "t"));
	CHECK(engine::core::FormatJson(out, 0) == "{}");
	tag.Remove(registry, e);
	CHECK_FALSE(registry.HasComponent<ScreenSpace>(e));

	CHECK(types.Find<Interpolation>()->IsRuntime());
}

TEST_CASE("Loading a component runs its OnAdd hook", "[ecs][typeregistry]")
{
	TypeRegistry types;
	RegisterBrain(types);
	Registry registry;
	int added = 0;
	registry.OnAdd<Brain>([&added](Entity, Brain&)
	{
		++added;
	});
	const Entity e = registry.Create();
	Issues issues;
	REQUIRE(types.Find("Brain")->Load(registry, e, Parse("{}"), FakeIds{}, issues, "p"));
	CHECK(added == 1);
}

TEST_CASE("Every engine component round-trips through JSON", "[ecs][typeregistry]")
{
	TypeRegistry types;
	engine::ecs::RegisterEngineComponents(types);
	Registry registry;
	const Entity parent = registry.Create();
	const Entity e = registry.Create();
	FakeIds ids;
	ids.byId = {{1, parent}, {2, e}};

	registry.Add<engine::ecs::Name>(e, engine::ecs::Name{"Player"});
	registry.Add<engine::ecs::Parent>(e, engine::ecs::Parent{parent});
	registry.Add<Transform>(e, Transform{{16.5f, 24.0f}});
	registry.Add<LocalTransform>(e, LocalTransform{{-1.25f, 6.0f}});
	registry.Add<Shape>(e, Shape{ShapeKind::Circle, 0, 0, 4.5f, Color{255, 220, 0}, 2});
	registry.Add<Sprite>(e, Sprite{{1, 2, 3, 4}, Color{10, 20, 30}, 5, true, 8, 9});
	registry.Add<engine::graphics::ClipRect>(e, engine::graphics::ClipRect{{0, 0, 64, 32}});
	registry.Add<ScreenSpace>(e);

	const Entity copy = registry.Create();
	for (std::size_t i = 0; i < types.Count(); ++i)
	{
		const auto& info = types.At(i);
		if (!info.Has(registry, e)) continue;
		JsonValue saved;
		Issues issues;
		REQUIRE(info.Save(registry, e, ids, saved, issues, info.Name()));
		REQUIRE(info.Load(registry, copy, saved, ids, issues, info.Name()));
		JsonValue again;
		REQUIRE(info.Save(registry, copy, ids, again, issues, info.Name()));
		CHECK(saved == again);
		CHECK(issues.Ok());
		CHECK(issues.warnings.empty());
	}
	CHECK(registry.GetComponent<Sprite>(copy).source == PixelRect{1, 2, 3, 4});
	CHECK(registry.GetComponent<Shape>(copy).color.g == 220);
}

TEST_CASE("Float fields reject out-of-range and non-finite values", "[ecs][typeregistry]")
{
	TypeRegistry types;
	RegisterBrain(types);
	Registry registry;
	const Entity e = registry.Create();
	const FakeIds ids;
	Issues issues;
	CHECK_FALSE(types.Find("Brain")->Load(registry, e, Parse(R"({"speed": 1e300})"), ids, issues, "b"));
	CHECK(issues.errors.size() == 1);

	registry.Add<Brain>(e, Brain{.speed = std::numeric_limits<float>::infinity()});
	JsonValue out;
	Issues saveIssues;
	CHECK_FALSE(types.Find("Brain")->Save(registry, e, ids, out, saveIssues, "b"));
	REQUIRE(saveIssues.errors.size() == 1);
	CHECK(saveIssues.errors[0].path == "b.speed");
}

TEST_CASE("Loading onto an entity that has the component replaces it", "[ecs][typeregistry]")
{
	TypeRegistry types;
	RegisterBrain(types);
	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Brain>(e, Brain{.level = 1, .label = "old"});
	Issues issues;
	REQUIRE(types.Find("Brain")->Load(registry, e, Parse(R"({"level": 5})"), FakeIds{}, issues, "b"));
	CHECK(registry.GetComponent<Brain>(e).level == 5);
	CHECK(registry.GetComponent<Brain>(e).label.empty());
}

TEST_CASE("A 64-bit unsigned value too big for JSON is an error, not an abort", "[ecs][typeregistry]")
{
	struct Big
	{
		std::uint64_t value = 0;
	};
	TypeRegistry types;
	types.Register<Big>("Big").Field("value", &Big::value);
	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Big>(e, Big{std::numeric_limits<std::uint64_t>::max()});
	JsonValue out;
	Issues issues;
	CHECK_FALSE(types.Find("Big")->Save(registry, e, FakeIds{}, out, issues, "big"));
	REQUIRE(issues.errors.size() == 1);
	CHECK(issues.errors[0].path == "big.value");
}

TEST_CASE("Malformed engine value types and non-objects are load errors", "[ecs][typeregistry]")
{
	TypeRegistry types;
	engine::ecs::RegisterEngineComponents(types);
	RegisterBrain(types);
	Registry registry;
	const Entity e = registry.Create();
	const FakeIds ids;

	const auto fails = [&](const char* component, const char* json)
	{
		Issues issues;
		const bool ok = types.Find(component)->Load(registry, e, Parse(json), ids, issues, "c");
		return !ok && !issues.errors.empty();
	};
	CHECK(fails("Shape", R"({"color": [1, 2]})"));
	CHECK(fails("Shape", R"({"color": [1, 2, 300]})"));
	CHECK(fails("Shape", R"({"color": "red"})"));
	CHECK(fails("Transform", R"({"position": 5})"));
	CHECK(fails("Transform", R"({"position": {"x": "a"}})"));
	CHECK(fails("ClipRect", R"({"rect": {"width": 1.5}})"));
	CHECK(fails("Brain", R"({"friends": 3})"));
	CHECK(fails("Brain", R"({"friends": [1, 99]})"));
	CHECK(fails("Brain", "[1, 2]"));
	CHECK_FALSE(registry.HasComponent<Shape>(e));
}

TEST_CASE("A failed load leaves the existing component untouched", "[ecs][typeregistry]")
{
	TypeRegistry types;
	RegisterBrain(types);
	Registry registry;
	const Entity e = registry.Create();
	registry.Add<Brain>(e, Brain{.level = 4});
	Issues issues;
	CHECK_FALSE(types.Find("Brain")->Load(registry, e, Parse(R"({"level": "x"})"), FakeIds{}, issues, "b"));
	CHECK(registry.GetComponent<Brain>(e).level == 4);
}
