#include "hud.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "assets.hpp"
#include "ecs/entity.hpp"
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

		void SpawnLabel(Registry& registry, std::string_view label, float anchorX, Align align, float y)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, Position{AlignedX(anchorX, align, Width(label)), y});
			registry.Add<Text>(entity, Text{.value = std::string(label), .color = kHudLabelColor, .layer = kHudLayer});
		}

		void SpawnField(Registry& registry, HudField field, float anchorX, Align align, float y, Color color)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, Position{anchorX, y});
			registry.Add<Text>(entity, Text{.color = color, .layer = kHudLayer});
			registry.Add<HudText>(entity, HudText{.field = field, .anchorX = anchorX, .align = align});
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
		switch (view.stage)
		{
		case Stage::Ready:
			return "READY!";
		case Stage::LevelComplete:
			return "WELL DONE!";
		case Stage::GameOver:
			return "GAME OVER";
		case Stage::Playing:
		case Stage::Dying:
			break;
		}
		return {};
	}

	float AlignedX(float anchorX, Align align, int width) noexcept
	{
		const auto pixels = static_cast<float>(width);
		switch (align)
		{
		case Align::Left:
			break;
		case Align::Centre:
			return std::floor(anchorX - pixels / 2.0f);
		case Align::Right:
			return std::floor(anchorX - pixels);
		}
		return std::floor(anchorX);
	}

	void SpawnHud(Registry& registry, const TileMap& map, Position origin)
	{
		constexpr float kLeft = kMargin;
		constexpr float kCentre = static_cast<float>(kLogicalWidth) / 2.0f;
		constexpr float kRight = static_cast<float>(kLogicalWidth) - kMargin;

		SpawnLabel(registry, "SCORE", kLeft, Align::Left, kTopLine);
		SpawnField(registry, HudField::Score, kLeft, Align::Left, kSecondLine, kHudValueColor);
		SpawnLabel(registry, "HIGH SCORE", kCentre, Align::Centre, kTopLine);
		SpawnField(registry, HudField::HighScore, kCentre, Align::Centre, kSecondLine, kHudValueColor);
		SpawnLabel(registry, "LEVEL", kRight, Align::Right, kTopLine);
		SpawnField(registry, HudField::Level, kRight, Align::Right, kSecondLine, kHudValueColor);
		SpawnField(registry, HudField::Lives, kLeft, Align::Left, kBottomLine, kHudLivesColor);

		// One pixel down from the top of the middle row, so the 7-pixel capitals sit centred in it.
		const float middleRow = origin.y + static_cast<float>(map.Height() / 2) * kTileSize + 1.0f;
		SpawnField(registry, HudField::Message, kCentre, Align::Centre, middleRow, kHudMessageColor);
	}

	void HudSystem(engine::ecs::World& world, const engine::ecs::FrameContext&)
	{
		const GameFlow& flow = world.resources.Get<GameFlow>();
		const int score = world.resources.Get<Score>().points;
		HighScore& high = world.resources.Get<HighScore>();
		high.points = std::max(high.points, score);

		const HudView view{
		    .score = score, .highScore = high.points, .level = flow.level, .lives = flow.lives, .stage = flow.stage};
		world.registry.Each<HudText, Text, Transform>([&view](Entity, const HudText& hud, Text& text, Transform& at)
		{
			text.value = HudLine(hud.field, view);
			at.position.x = AlignedX(hud.anchorX, hud.align, Width(text.value));
		});
	}
} // namespace game
