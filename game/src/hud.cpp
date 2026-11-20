#include "hud.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

#include "assets.hpp"
#include "ecs/entity.hpp"
#include "ecs/states.hpp"
#include "graphics/font.hpp"
#include "maze.hpp"
#include "mazegame.hpp"
#include "scoring.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::Entity;
		using engine::ecs::Registry;
		using engine::graphics::Align;
		using engine::graphics::AlignedX;
		using engine::graphics::Color;
		using engine::graphics::Position;
		using engine::graphics::Text;
		using engine::graphics::Transform;

		constexpr float kMargin = 16.0f;
		constexpr float kTopLine = 0.0f;
		constexpr float kSecondLine = 8.0f;
		constexpr float kBottomLine = static_cast<float>(kLogicalHeight) - 8.0f;

		[[nodiscard]] int Width(std::string_view text) noexcept
		{
			return static_cast<int>(text.size()) * kGlyphAdvance;
		}

		Entity SpawnLabel(Registry& registry, std::string_view label, float anchorX, Align align, float y)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, Position{AlignedX(anchorX, align, Width(label)), y});
			registry.Add<HudLabel>(entity);
			registry.Add<Text>(entity, Text{.value = std::string(label), .color = kHudLabelColor, .layer = kHudLayer});
			return entity;
		}

		Entity SpawnField(Registry& registry, HudField field, float anchorX, Align align, float y, Color color)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, Position{anchorX, y});
			registry.Add<Text>(entity, Text{.color = color, .layer = kHudLayer});
			registry.Add<HudText>(entity, HudText{.field = field, .anchorX = anchorX, .align = align});
			return entity;
		}
	} // namespace

	std::string HudLine(HudField field, const HudView& view)
	{
		switch (field)
		{
		case HudField::Score:
			return std::to_string(view.score);
		case HudField::HighScore:
			return std::to_string(view.highScore);
		case HudField::Level:
			return std::to_string(view.level);
		case HudField::Lives:
			return std::string(static_cast<std::size_t>(std::max(view.lives, 0)), '@');
		case HudField::Message:
			break;
		}
		if (view.screen == Screen::Playing && view.round == Round::Ready) return "READY!";
		if (view.screen == Screen::GameOver) return "GAME OVER";
		return {};
	}

	std::vector<Entity> SpawnHudScene(Registry& registry, const TileMap& map, Position origin)
	{
		const HudView sample{.lives = 3, .screen = Screen::Playing, .round = Round::Ready};
		std::vector<Entity> made;
		const auto label = [&](std::string_view text, float x, Align align, float y)
		{
			made.push_back(SpawnLabel(registry, text, x, align, y));
		};
		const auto field = [&](HudField which, float x, Align align, float y, Color color)
		{
			const Entity entity = SpawnField(registry, which, x, align, y, color);
			registry.GetComponent<Text>(entity).value = HudLine(which, sample); // something to see in the editor
			registry.GetComponent<Transform>(entity).position.x = AlignedX(x, align, Width(registry.GetComponent<Text>(entity).value));
			made.push_back(entity);
		};
		constexpr float kLeft = kMargin;
		constexpr float kCentre = static_cast<float>(kLogicalWidth) / 2.0f;
		constexpr float kRight = static_cast<float>(kLogicalWidth) - kMargin;

		label("SCORE", kLeft, Align::Left, kTopLine);
		field(HudField::Score, kLeft, Align::Left, kSecondLine, kHudValueColor);
		label("HIGH SCORE", kCentre, Align::Centre, kTopLine);
		field(HudField::HighScore, kCentre, Align::Centre, kSecondLine, kHudValueColor);
		label("LEVEL", kRight, Align::Right, kTopLine);
		field(HudField::Level, kRight, Align::Right, kSecondLine, kHudValueColor);
		field(HudField::Lives, kLeft, Align::Left, kBottomLine, kHudLivesColor);

		// One pixel down from the top of the middle row, so the 7-pixel capitals sit centred in it.
		const float middleRow = origin.y + static_cast<float>(map.Height() / 2) * kTileSize + 1.0f;
		field(HudField::Message, kCentre, Align::Centre, middleRow, kHudMessageColor);
		return made;
	}

	void SpawnHud(Registry& registry, const TileMap& map, Position origin)
	{
		for (const Entity entity : SpawnHudScene(registry, map, origin))
			registry.Add<engine::ecs::DespawnOnEnter<Screen>>(entity, WorldScope());
	}

	bool HasSceneHud(Registry& registry)
	{
		bool found = false;
		registry.Each<HudText>([&found](Entity, const HudText&)
		{
			found = true;
		});
		return found;
	}

	void ScopeSceneHud(Registry& registry)
	{
		std::vector<Entity> texts;
		registry.Each<HudText>([&texts](Entity entity, const HudText&)
		{
			texts.push_back(entity);
		});
		registry.Each<HudLabel>([&texts](Entity entity, const HudLabel&)
		{
			texts.push_back(entity);
		});
		for (const Entity entity : texts)
			registry.Add<engine::ecs::DespawnOnEnter<Screen>>(entity, WorldScope());
	}

	void HudSystem(engine::ecs::World& world, const engine::ecs::FrameContext&)
	{
		const GameFlow& flow = world.resources.Get<GameFlow>();
		const int score = world.resources.Get<Score>().points;
		HighScore& high = world.resources.Get<HighScore>();
		high.points = std::max(high.points, score);

		const auto& screen = world.resources.Get<engine::ecs::State<Screen>>();
		const auto& round = world.resources.Get<engine::ecs::State<Round>>();
		const HudView view{.score = score,
		                   .highScore = high.points,
		                   .level = flow.level,
		                   .lives = flow.lives,
		                   .screen = screen.value,
		                   .round = round.value};
		world.registry.Each<HudText, Text, Transform>([&view](Entity, const HudText& hud, Text& text, Transform& at)
		{
			text.value = HudLine(hud.field, view);
			at.position.x = AlignedX(hud.anchorX, hud.align, Width(text.value));
		});
	}
} // namespace game
