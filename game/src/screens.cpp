#include "screens.hpp"

#include <algorithm>
#include <iterator>

#include "assets.hpp"
#include "controls.hpp"
#include "ecs/entity.hpp"
#include "events.hpp"
#include "gamestate.hpp"
#include "graphics/color.hpp"
#include "graphics/font.hpp"
#include "graphics/transform.hpp"
#include "hud.hpp"
#include "mazegame.hpp"
#include "systems.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::Entity;
		using engine::ecs::Registry;
		using engine::ecs::World;
		using engine::graphics::Color;
		using engine::graphics::Position;
		using engine::graphics::Text;
		using engine::graphics::Transform;

		constexpr float kCentre = static_cast<float>(kLogicalWidth) / 2.0f;
		constexpr Color kTitleColor{255, 200, 70};
		constexpr Color kItemColor{255, 255, 255};
		constexpr Color kHintColor{120, 200, 255};
		constexpr std::size_t kNoItem = static_cast<std::size_t>(-1);

		[[nodiscard]] int Width(std::string_view text, int scale = 1) noexcept
		{
			return static_cast<int>(text.size()) * kGlyphAdvance * scale;
		}

		void SpawnText(Registry& registry, Screen screen, std::string_view label, Position at, Color color,
		               int scale = 1, std::size_t item = kNoItem)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, at);
			registry.Add<Text>(entity, Text{.color = color, .layer = kMenuLayer, .scale = scale});
			registry.Add<ScreenText>(entity, ScreenText{.screen = screen, .label = std::string(label), .item = item});
		}

		void SpawnCentred(Registry& registry, Screen screen, std::string_view label, float y, Color color,
		                  int scale = 1)
		{
			SpawnText(registry, screen, label, {AlignedX(kCentre, Align::Centre, Width(label, scale)), y}, color,
			          scale);
		}

		// Menu items, one per line from y, in a column centred on the widest (with its cursor).
		template <std::size_t N>
		void SpawnMenu(Registry& registry, Screen screen, const std::string_view (&labels)[N], float y)
		{
			std::size_t widest = 0;
			for (const std::string_view label : labels)
			{
				widest = std::max(widest, MenuLine(label, true).size());
			}
			const float left = AlignedX(kCentre, Align::Centre, Width(std::string(widest, ' ')));
			for (std::size_t i = 0; i < N; ++i)
			{
				SpawnText(registry, screen, labels[i], {left, y + static_cast<float>(i) * 12.0f}, kItemColor, 1, i);
			}
		}
	} // namespace

	std::size_t MoveSelection(std::size_t selected, std::size_t count, bool up, bool down) noexcept
	{
		if (count == 0) return 0;
		selected = std::min(selected, count - 1);
		if (up && !down) return selected == 0 ? count - 1 : selected - 1;
		if (down && !up) return selected + 1 == count ? 0 : selected + 1;
		return selected;
	}

	bool CanPause(Stage stage) noexcept
	{
		return stage != Stage::Title && stage != Stage::GameOver;
	}

	ScreenStep StepScreens(MenuCursor cursor, Stage stage, bool paused, const ScreenInput& input) noexcept
	{
		if (stage == Stage::Title)
		{
			cursor.title = MoveSelection(cursor.title, kTitleItems, input.up, input.down);
			if (!input.confirm) return {.cursor = cursor};
			const auto item = static_cast<TitleItem>(cursor.title);
			return {.cursor = cursor,
			        .action = item == TitleItem::Start ? ScreenAction::StartGame : ScreenAction::Quit};
		}
		if (paused)
		{
			if (input.pause) return {.cursor = cursor, .action = ScreenAction::Resume};
			cursor.pause = MoveSelection(cursor.pause, kPauseItems, input.up, input.down);
			if (!input.confirm) return {.cursor = cursor};
			const auto item = static_cast<PauseItem>(cursor.pause);
			return {.cursor = cursor,
			        .action = item == PauseItem::Resume ? ScreenAction::Resume : ScreenAction::QuitToTitle};
		}
		if (input.pause && CanPause(stage))
		{
			cursor.pause = 0;
			return {.cursor = cursor, .action = ScreenAction::Pause};
		}
		return {.cursor = cursor};
	}

	std::string MenuLine(std::string_view label, bool selected)
	{
		return (selected ? "> " : "  ") + std::string(label);
	}

	void SpawnScreens(Registry& registry)
	{
		SpawnCentred(registry, Screen::Title, kGameTitle, 72.0f, kTitleColor, 2);
		constexpr std::string_view kTitleLabels[] = {"START", "QUIT"};
		static_assert(std::size(kTitleLabels) == kTitleItems);
		SpawnMenu(registry, Screen::Title, kTitleLabels, 128.0f);
		SpawnCentred(registry, Screen::Title, "ARROWS OR WASD: MOVE", 200.0f, kHintColor);
		SpawnCentred(registry, Screen::Title, "P OR ESC: PAUSE", 212.0f, kHintColor);
		SpawnCentred(registry, Screen::Title, "ENTER OR SPACE: CHOOSE", 224.0f, kHintColor);

		SpawnCentred(registry, Screen::Pause, "PAUSED", 104.0f, kTitleColor, 2);
		constexpr std::string_view kPauseLabels[] = {"CONTINUE", "QUIT TO TITLE"};
		static_assert(std::size(kPauseLabels) == kPauseItems);
		SpawnMenu(registry, Screen::Pause, kPauseLabels, 140.0f);
	}

	void ScreenSystem(World& world, const engine::ecs::FrameContext&)
	{
		const auto& controls = world.resources.Get<Controls>();
		GameState& state = world.resources.Get<GameState>();
		MenuCursor& cursor = world.resources.Get<MenuCursor>();
		const GameFlow& flow = world.resources.Get<GameFlow>();

		const ScreenInput input{.up = controls.WasPressed(Action::MoveUp),
		                        .down = controls.WasPressed(Action::MoveDown),
		                        .confirm = controls.WasPressed(Action::Confirm),
		                        .pause = controls.WasPressed(Action::Pause)};
		const ScreenStep step = StepScreens(cursor, flow.stage, state.paused, input);
		cursor = step.cursor;

		switch (step.action)
		{
		case ScreenAction::None:
			break;
		case ScreenAction::StartGame:
			SetFlow(world, StartGame(flow));
			break;
		case ScreenAction::Quit:
			state.quitRequested = true;
			break;
		case ScreenAction::Pause:
			state.paused = true;
			break;
		case ScreenAction::Resume:
			state.paused = false;
			break;
		case ScreenAction::QuitToTitle:
			state.paused = false;
			SetFlow(world, ReturnToTitle(flow));
			break;
		}
	}

	void ScreenTextSystem(World& world, const engine::ecs::FrameContext&)
	{
		const bool onTitle = world.resources.Get<GameFlow>().stage == Stage::Title;
		const bool paused = world.resources.Get<GameState>().paused;
		const MenuCursor& cursor = world.resources.Get<MenuCursor>();

		world.registry.Each<ScreenText, Text>([&](Entity, const ScreenText& screen, Text& text)
		{
			const bool up = screen.screen == Screen::Title ? onTitle : paused && !onTitle;
			const std::size_t selected = screen.screen == Screen::Title ? cursor.title : cursor.pause;
			if (!up)
			{
				text.value.clear();
			}
			else if (screen.item == kNoItem)
			{
				text.value = screen.label;
			}
			else
			{
				text.value = MenuLine(screen.label, screen.item == selected);
			}
		});
	}
} // namespace game
