#include <catch2/catch_test_macros.hpp>

#include <string>

#include "ecs/enginecomponents.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/name.hpp"
#include "ecs/registry.hpp"
#include "ecs/typeregistry.hpp"
#include "editor/entityops.hpp"
#include "graphics/transform.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::Parent;
	using engine::ecs::Registry;
	using engine::editor::CreateEntity;

	// A game component that points at another entity.
	struct Target
	{
		Entity who = Entity::Null;
		int weight = 1;
	};

	engine::ecs::TypeRegistry MakeTypes()
	{
		engine::ecs::TypeRegistry types;
		engine::ecs::RegisterEngineComponents(types);
		types.Register<Target>("Target").Field("who", &Target::who).Field("weight", &Target::weight);
		return types;
	}
} // namespace

TEST_CASE("Entities are labelled by name, or by index when they have none", "[editor][ops]")
{
	Registry registry;
	const Entity named = CreateEntity(registry, "Player");
	const Entity bare = registry.Create();
	CHECK(engine::editor::LabelOf(registry, named) == "Player");
	CHECK(engine::editor::LabelOf(registry, bare) == "Entity " + std::to_string(bare.id));
}

TEST_CASE("SetParent moves entities and refuses loops", "[editor][ops]")
{
	Registry registry;
	const Entity a = CreateEntity(registry, "a");
	const Entity b = CreateEntity(registry, "b", a);
	const Entity c = CreateEntity(registry, "c", b);

	CHECK(registry.TryGetComponent<Parent>(b)->entity == a);
	CHECK(engine::editor::WouldMakeLoop(registry, a, c)); // a under its own grandchild
	CHECK(engine::editor::WouldMakeLoop(registry, a, a));
	CHECK_FALSE(engine::editor::SetParent(registry, a, c));
	CHECK_FALSE(registry.HasComponent<Parent>(a));

	CHECK(engine::editor::SetParent(registry, c, a)); // up a level
	CHECK(registry.TryGetComponent<Parent>(c)->entity == a);
	CHECK(engine::editor::SetParent(registry, c, Entity::Null)); // to the top
	CHECK_FALSE(registry.HasComponent<Parent>(c));
	CHECK(engine::editor::SetParent(registry, c, b)); // where there was no Parent before
	CHECK(registry.TryGetComponent<Parent>(c)->entity == b);
}

TEST_CASE("DestroyTree removes an entity and everything under it", "[editor][ops]")
{
	Registry registry;
	const Entity root = CreateEntity(registry, "root");
	const Entity child = CreateEntity(registry, "child", root);
	const Entity grandchild = CreateEntity(registry, "grandchild", child);
	const Entity other = CreateEntity(registry, "other");

	engine::editor::DestroyTree(registry, root);
	CHECK_FALSE(registry.IsAlive(root));
	CHECK_FALSE(registry.IsAlive(child));
	CHECK_FALSE(registry.IsAlive(grandchild));
	CHECK(registry.IsAlive(other));
}

TEST_CASE("DuplicateTree copies components and points inner references at the copies", "[editor][ops]")
{
	const engine::ecs::TypeRegistry types = MakeTypes();
	Registry registry;
	const Entity outside = CreateEntity(registry, "outside");
	const Entity root = CreateEntity(registry, "root");
	const Entity child = CreateEntity(registry, "child", root);
	registry.Add<engine::graphics::Transform>(child, engine::graphics::Transform{{3.0F, 4.0F}});
	registry.Add<Target>(root, Target{child, 7});    // inside the copied tree
	registry.Add<Target>(child, Target{outside, 2}); // outside it

	engine::ecs::Issues issues;
	const Entity copy = engine::editor::DuplicateTree(registry, types, root, issues);
	REQUIRE(issues.Ok());
	REQUIRE_FALSE(copy.IsNull());
	CHECK(copy != root);
	CHECK(engine::editor::LabelOf(registry, copy) == "root (copy)");
	CHECK(registry.EntityCount() == 5); // outside, root, child and the two copies

	const auto copiedChildren = engine::ecs::ChildrenOf(registry, copy);
	REQUIRE(copiedChildren.size() == 1);
	const Entity copiedChild = copiedChildren.front();
	CHECK(copiedChild != child);
	CHECK(engine::editor::LabelOf(registry, copiedChild) == "child");
	CHECK(registry.TryGetComponent<engine::graphics::Transform>(copiedChild)->position.x == 3.0F);

	CHECK(registry.TryGetComponent<Target>(copy)->who == copiedChild); // inner reference follows the copy
	CHECK(registry.TryGetComponent<Target>(copy)->weight == 7);
	CHECK(registry.TryGetComponent<Target>(copiedChild)->who == outside); // outer reference unchanged
	CHECK(registry.TryGetComponent<Target>(root)->who == child);          // the original is untouched
}

TEST_CASE("A duplicate of a child sits beside the original", "[editor][ops]")
{
	const engine::ecs::TypeRegistry types = MakeTypes();
	Registry registry;
	const Entity parent = CreateEntity(registry, "parent");
	const Entity child = CreateEntity(registry, "child", parent);

	engine::ecs::Issues issues;
	const Entity copy = engine::editor::DuplicateTree(registry, types, child, issues);
	REQUIRE_FALSE(copy.IsNull());
	CHECK(registry.TryGetComponent<Parent>(copy)->entity == parent);
	CHECK(engine::ecs::ChildrenOf(registry, parent).size() == 2);
}

TEST_CASE("Live ids map indexes to the live entities and honour overrides", "[editor][ops]")
{
	Registry registry;
	const Entity a = registry.Create();
	const Entity b = registry.Create();
	engine::editor::LiveIds ids(registry);
	CHECK(*ids.IdOf(a) == static_cast<std::int64_t>(a.id));
	CHECK(*ids.EntityOf(static_cast<std::int64_t>(b.id)) == b);
	CHECK_FALSE(ids.EntityOf(-1).has_value());
	CHECK_FALSE(ids.EntityOf(1000).has_value());
	ids.Override(a.id, b);
	CHECK(*ids.EntityOf(static_cast<std::int64_t>(a.id)) == b);
	registry.Destroy(a);
	CHECK_FALSE(ids.IdOf(a).has_value());
}
