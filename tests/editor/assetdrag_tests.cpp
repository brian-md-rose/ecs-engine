#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "ecs/name.hpp"
#include "ecs/registry.hpp"
#include "editor/assetdrag.hpp"
#include "editor/entityops.hpp"
#include "graphics/sprite.hpp"
#include "graphics/transform.hpp"

namespace
{
	using engine::editor::AssetDrag;
	using engine::editor::DecodeAssetDrag;
	using engine::editor::DragKind;
	using engine::editor::EncodeAssetDrag;
} // namespace

TEST_CASE("an asset drag survives the payload round trip, whatever its kind")
{
	for (const DragKind kind : {DragKind::Sprite, DragKind::Font, DragKind::Sound, DragKind::Scene, DragKind::Prefab})
	{
		const std::vector<char> bytes = EncodeAssetDrag({kind, "music/theme.ogg"});
		const auto back = DecodeAssetDrag(bytes);
		REQUIRE(back.has_value());
		CHECK(back->kind == kind);
		CHECK(back->name == "music/theme.ogg");
	}
}

TEST_CASE("a payload that is not an asset drag is refused")
{
	CHECK_FALSE(DecodeAssetDrag({}).has_value());
	const std::vector<char> onlyKind{static_cast<char>(DragKind::Sprite)};
	CHECK_FALSE(DecodeAssetDrag(onlyKind).has_value());
	const std::vector<char> badKind{static_cast<char>(99), 'a'};
	CHECK_FALSE(DecodeAssetDrag(badKind).has_value());
}

TEST_CASE("a dropped sprite becomes a named entity centred on the pointer")
{
	engine::ecs::Registry registry;
	const engine::graphics::PixelRect source{16, 8, 10, 6};
	const auto entity = engine::editor::CreateSpriteEntity(registry, "coin", source, {100.0F, 50.0F});

	REQUIRE(registry.IsAlive(entity));
	const auto* name = registry.TryGetComponent<engine::ecs::Name>(entity);
	REQUIRE(name != nullptr);
	CHECK(name->value == "coin");
	const auto* transform = registry.TryGetComponent<engine::graphics::Transform>(entity);
	REQUIRE(transform != nullptr);
	CHECK(transform->position.x == 95.0F);
	CHECK(transform->position.y == 47.0F);
	const auto* sprite = registry.TryGetComponent<engine::graphics::Sprite>(entity);
	REQUIRE(sprite != nullptr);
	CHECK(sprite->source == source);
}
