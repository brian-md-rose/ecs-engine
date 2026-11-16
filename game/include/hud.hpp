#pragma once
#include <cstdint>
#include <string>

#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "gameflow.hpp"
#include "graphics/color.hpp"
#include "graphics/font.hpp"
#include "graphics/transform.hpp"
#include "tilemap.hpp"

// The heads-up display, drawn with the bitmap font: score, high score and level across the top,
// lives at the bottom, and a message (READY!, GAME OVER) across the middle of the maze.
namespace game
{
	inline constexpr std::uint8_t kHudLayer = 10; // above everything in the maze

	inline constexpr engine::graphics::Color kHudLabelColor{120, 200, 255};
	inline constexpr engine::graphics::Color kHudValueColor{255, 255, 255};
	inline constexpr engine::graphics::Color kHudMessageColor{255, 220, 90};
	inline constexpr engine::graphics::Color kHudLivesColor{80, 230, 210};

	// Resource: the best score. points is what the HUD shows, and follows the score up while a game
	// is played; record is the best of finished games, set by whoever name is, and what is saved.
	struct HighScore
	{
		int points = 0;
		int record = 0;
		std::string name{}; // who set the record; empty if nobody has
	};

	// What a piece of the HUD shows.
	enum class HudField : std::uint8_t
	{
		Score,
		HighScore,
		Level,
		Lives,
		Message
	};

	// Component: a Text the HUD keeps up to date, placed against anchorX each time it changes
	// (engine::graphics::AlignedX).
	struct HudText
	{
		HudField field = HudField::Score;
		float anchorX = 0.0f;
		engine::graphics::Align align = engine::graphics::Align::Left;
	};

	// Everything the HUD reads, gathered from the resources.
	struct HudView
	{
		int score = 0;
		int highScore = 0;
		int level = 1;
		int lives = 0;
		Screen screen = Screen::Playing;
		Round round = Round::Active; // only meaningful while the screen is Playing
	};

	// The text for one field, in capitals (the font has no small letters). Lives are one '@'
	// icon per try left; the message is empty when the game has none. Pure.
	[[nodiscard]] std::string HudLine(HudField field, const HudView& view);

	// Setup: creates the HUD's labels and fields, the message centred across the middle row of
	// a maze drawn at origin. Creates entities directly, so call it during setup.
	void SpawnHud(engine::ecs::Registry& registry, const TileMap& map, engine::graphics::Position origin);

	// LateUpdate (always, so the HUD shows the state after this update): raises the HighScore
	// to the score, and updates every HudText's Text and Transform.
	void HudSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
