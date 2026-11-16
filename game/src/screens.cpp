#include "screens.hpp"

#include <algorithm>
#include <iterator>

#include "assets.hpp"
#include "controls.hpp"
#include "ecs/entity.hpp"
#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "graphics/color.hpp"
#include "graphics/font.hpp"
#include "graphics/transform.hpp"
#include "hud.hpp"
#include "mazegame.hpp"
#include "nameentry.hpp"
#include "options.hpp"
#include "save.hpp"
#include "scoring.hpp"
#include "systems.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::DespawnOnExit;
		using engine::ecs::Entity;
		using engine::ecs::NextState;
		using engine::ecs::Registry;
		using engine::ecs::State;
		using engine::ecs::World;
		using engine::graphics::Align;
		using engine::graphics::AlignedX;
		using engine::graphics::Color;
		using engine::graphics::Position;
		using engine::graphics::Text;
		using engine::graphics::Transform;

		constexpr float kCentre = static_cast<float>(kLogicalWidth) / 2.0f;
		constexpr float kLine = 12.0f; // from one menu line to the next
		constexpr Color kTitleColor{255, 200, 70};
		constexpr Color kItemColor{255, 255, 255};
		constexpr Color kHintColor{120, 200, 255};
		constexpr Color kSlotColor{255, 255, 255};
		constexpr Color kCursorSlotColor{255, 200, 70};

		// The y of the name entry's row of slots, and the distance from one slot to the next.
		constexpr float kSlotY = 140.0f;
		constexpr float kSlotPitch = 16.0f;
		constexpr int kSlotScale = 2;

		[[nodiscard]] int Width(std::string_view text, int scale = 1) noexcept
		{
			return static_cast<int>(text.size()) * kGlyphAdvance * scale;
		}

		// A text entity that goes when T's state leaves scope.
		template <typename T>
		Entity SpawnText(Registry& registry, T scope, std::string_view value, Position at, Color color, int scale = 1)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, at);
			registry.Add<Text>(entity,
			                   Text{.value = std::string(value), .color = color, .layer = kMenuLayer, .scale = scale});
			registry.Add<DespawnOnExit<T>>(entity, DespawnOnExit<T>{scope});
			return entity;
		}

		template <typename T>
		void SpawnCentred(Registry& registry, T scope, std::string_view value, float y, Color color, int scale = 1)
		{
			SpawnText(registry, scope, value, {AlignedX(kCentre, Align::Centre, Width(value, scale)), y}, color, scale);
		}

		// Menu items, one per line from y, in a column centred on the widest (with its cursor),
		// each a ScreenText of the given kind.
		template <typename T>
		void SpawnMenu(Registry& registry, T scope, TextKind kind, const std::string_view* labels, std::size_t count,
		               float y)
		{
			std::size_t widest = 0;
			for (std::size_t i = 0; i < count; ++i)
			{
				widest = std::max(widest, MenuLine(labels[i], true).size());
			}
			const float left = AlignedX(kCentre, Align::Centre, Width(std::string(widest, ' ')));
			for (std::size_t i = 0; i < count; ++i)
			{
				const Entity entity =
				    SpawnText(registry, scope, labels[i], {left, y + static_cast<float>(i) * kLine}, kItemColor);
				registry.Add<ScreenText>(entity, ScreenText{.kind = kind, .label = std::string(labels[i]), .item = i});
			}
		}

		[[nodiscard]] Controls& Pad(World& world)
		{
			return world.resources.Get<Controls>();
		}

		void RequestPause(World& world, Pause value)
		{
			world.resources.Get<NextState<Pause>>().Set(value);
		}

		void RequestScreen(World& world, Screen value)
		{
			world.resources.Get<NextState<Screen>>().Set(value);
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

	std::string MenuLine(std::string_view label, bool selected)
	{
		return (selected ? "> " : "  ") + std::string(label);
	}

	void SpawnTitleScreen(World& world, const engine::ecs::FrameContext&)
	{
		Registry& registry = world.registry;
		constexpr Screen kScope = Screen::Title;
		SpawnCentred(registry, kScope, kGameTitle, 72.0f, kTitleColor, 2);
		constexpr std::string_view kLabels[] = {"START", "OPTIONS", "QUIT"};
		static_assert(std::size(kLabels) == kTitleItems);
		SpawnMenu(registry, kScope, TextKind::TitleItem, kLabels, std::size(kLabels), 128.0f);

		// Whoever holds the record, once there is one.
		const HighScore& high = world.resources.Get<HighScore>();
		if (high.record > 0)
		{
			SpawnCentred(registry, kScope, "HIGH SCORE", 172.0f, kHintColor);
			const std::string holder = (high.name.empty() ? "" : high.name + "  ") + std::to_string(high.record);
			SpawnCentred(registry, kScope, holder, 184.0f, kItemColor);
		}

		SpawnCentred(registry, kScope, "ARROWS OR WASD: MOVE", 200.0f, kHintColor);
		SpawnCentred(registry, kScope, "P OR ESC: PAUSE", 212.0f, kHintColor);
		SpawnCentred(registry, kScope, "ENTER OR SPACE: CHOOSE", 224.0f, kHintColor);
	}

	void SpawnOptionsScreen(World& world, const engine::ecs::FrameContext&)
	{
		Registry& registry = world.registry;
		constexpr Screen kScope = Screen::Options;
		world.resources.Get<MenuCursor>().options = 0;
		SpawnCentred(registry, kScope, "OPTIONS", 72.0f, kTitleColor, 2);

		// The labels are placeholders: ScreenTextSystem writes each line from the settings.
		constexpr std::string_view kLabels[] = {"SOUND: OFF", "LIVES: 3", "BACK"};
		static_assert(std::size(kLabels) == kOptionItems);
		SpawnMenu(registry, kScope, TextKind::OptionItem, kLabels, std::size(kLabels), 120.0f);

		SpawnCentred(registry, kScope, "UP DOWN: CHOOSE", 188.0f, kHintColor);
		SpawnCentred(registry, kScope, "LEFT RIGHT OR ENTER: CHANGE", 200.0f, kHintColor);
		SpawnCentred(registry, kScope, "ESC: BACK", 212.0f, kHintColor);
	}

	void SpawnNameEntryScreen(World& world, const engine::ecs::FrameContext&)
	{
		Registry& registry = world.registry;
		constexpr Screen kScope = Screen::NameEntry;
		world.resources.Get<NameEntry>().cursor = 0;
		SpawnCentred(registry, kScope, "NEW HIGH SCORE!", 64.0f, kTitleColor, 2);
		SpawnCentred(registry, kScope, "SCORE " + std::to_string(world.resources.Get<Score>().points), 92.0f,
		             kItemColor);
		SpawnCentred(registry, kScope, "ENTER YOUR NAME", 116.0f, kHintColor);

		const float left = kCentre - (static_cast<float>(kNameLength) * kSlotPitch) / 2.0f;
		for (std::size_t i = 0; i < kNameLength; ++i)
		{
			const Entity slot = SpawnText(registry, kScope, "-", {left + static_cast<float>(i) * kSlotPitch, kSlotY},
			                              kSlotColor, kSlotScale);
			registry.Add<ScreenText>(slot, ScreenText{.kind = TextKind::NameSlot, .item = i});
		}

		SpawnCentred(registry, kScope, "UP DOWN: CHANGE LETTER", 188.0f, kHintColor);
		SpawnCentred(registry, kScope, "LEFT RIGHT: MOVE", 200.0f, kHintColor);
		SpawnCentred(registry, kScope, "ENTER: DONE", 212.0f, kHintColor);
	}

	void SpawnLevelTransitionScreen(World& world, const engine::ecs::FrameContext&)
	{
		Registry& registry = world.registry;
		constexpr Screen kScope = Screen::LevelTransition;
		SpawnCentred(registry, kScope, "WELL DONE!", 92.0f, kHintColor);
		SpawnCentred(registry, kScope, "LEVEL " + std::to_string(world.resources.Get<GameFlow>().level), 112.0f,
		             kTitleColor, 2);
		SpawnCentred(registry, kScope, "SCORE " + std::to_string(world.resources.Get<Score>().points), 144.0f,
		             kItemColor);
	}

	void SpawnPauseMenu(World& world, const engine::ecs::FrameContext&)
	{
		Registry& registry = world.registry;
		constexpr Pause kScope = Pause::Paused;
		world.resources.Get<MenuCursor>().pause = 0;
		SpawnCentred(registry, kScope, "PAUSED", 104.0f, kTitleColor, 2);
		constexpr std::string_view kLabels[] = {"CONTINUE", "QUIT TO TITLE"};
		static_assert(std::size(kLabels) == kPauseItems);
		SpawnMenu(registry, kScope, TextKind::PauseItem, kLabels, std::size(kLabels), 140.0f);
	}

	void TitleSystem(World& world, const engine::ecs::FrameContext&)
	{
		const Controls& controls = Pad(world);
		MenuCursor& cursor = world.resources.Get<MenuCursor>();
		cursor.title = MoveSelection(cursor.title, kTitleItems, controls.WasPressed(Action::MoveUp),
		                             controls.WasPressed(Action::MoveDown));
		if (!controls.WasPressed(Action::Confirm)) return;

		switch (static_cast<TitleItem>(cursor.title))
		{
		case TitleItem::Start:
			StartNewGame(world);
			RequestScreen(world, Screen::Playing);
			break;
		case TitleItem::Options:
			RequestScreen(world, Screen::Options);
			break;
		case TitleItem::Quit:
			world.resources.Get<GameState>().quitRequested = true;
			break;
		case TitleItem::Count:
			break;
		}
	}

	void OptionsSystem(World& world, const engine::ecs::FrameContext&)
	{
		const Controls& controls = Pad(world);
		if (controls.WasPressed(Action::Pause))
		{
			RequestScreen(world, Screen::Title);
			return;
		}

		MenuCursor& cursor = world.resources.Get<MenuCursor>();
		cursor.options = MoveSelection(cursor.options, kOptionItems, controls.WasPressed(Action::MoveUp),
		                               controls.WasPressed(Action::MoveDown));
		const auto item = static_cast<OptionItem>(cursor.options);

		if (item == OptionItem::Back)
		{
			if (controls.WasPressed(Action::Confirm)) RequestScreen(world, Screen::Title);
			return;
		}
		const bool left = controls.WasPressed(Action::MoveLeft);
		const bool right = controls.WasPressed(Action::MoveRight) || controls.WasPressed(Action::Confirm);
		if (left != right)
		{
			Settings& settings = world.resources.Get<Settings>();
			settings = ChangeSetting(settings, item, right ? 1 : -1);
		}
	}

	void NameEntrySystem(World& world, const engine::ecs::FrameContext&)
	{
		const Controls& controls = Pad(world);
		NameEntry& entry = world.resources.Get<NameEntry>();
		entry = StepNameEntry(entry, {.up = controls.WasPressed(Action::MoveUp),
		                              .down = controls.WasPressed(Action::MoveDown),
		                              .left = controls.WasPressed(Action::MoveLeft),
		                              .right = controls.WasPressed(Action::MoveRight)});
		if (!controls.WasPressed(Action::Confirm)) return;

		RecordScore(world, NameOf(entry));
		SaveHighScore(world);
		RequestScreen(world, Screen::GameOver);
	}

	void GameOverSystem(World& world, const engine::ecs::FrameContext&)
	{
		if (Pad(world).WasPressed(Action::Confirm)) RequestScreen(world, Screen::Title);
	}

	void PauseSystem(World& world, const engine::ecs::FrameContext&)
	{
		const Controls& controls = Pad(world);
		const bool paused = world.resources.Get<State<Pause>>().Is(Pause::Paused);
		const bool pause = controls.WasPressed(Action::Pause);
		if (!paused)
		{
			if (pause) RequestPause(world, Pause::Paused);
			return;
		}

		MenuCursor& cursor = world.resources.Get<MenuCursor>();
		if (pause)
		{
			RequestPause(world, Pause::Running);
			return;
		}
		cursor.pause = MoveSelection(cursor.pause, kPauseItems, controls.WasPressed(Action::MoveUp),
		                             controls.WasPressed(Action::MoveDown));
		if (!controls.WasPressed(Action::Confirm)) return;
		if (static_cast<PauseItem>(cursor.pause) == PauseItem::Resume)
		{
			RequestPause(world, Pause::Running);
		}
		else
		{
			AbandonGame(world);
		}
	}

	void ScreenTextSystem(World& world, const engine::ecs::FrameContext&)
	{
		const MenuCursor& cursor = world.resources.Get<MenuCursor>();
		const Settings& settings = world.resources.Get<Settings>();
		const NameEntry& entry = world.resources.Get<NameEntry>();

		world.registry.Each<ScreenText, Text>([&](Entity, const ScreenText& screen, Text& text)
		{
			switch (screen.kind)
			{
			case TextKind::TitleItem:
				text.value = MenuLine(screen.label, screen.item == cursor.title);
				break;
			case TextKind::PauseItem:
				text.value = MenuLine(screen.label, screen.item == cursor.pause);
				break;
			case TextKind::OptionItem:
				text.value =
				    MenuLine(OptionText(static_cast<OptionItem>(screen.item), settings), screen.item == cursor.options);
				break;
			case TextKind::NameSlot:
			{
				const char c = screen.item < entry.slots.size() ? entry.slots[screen.item] : ' ';
				text.value = std::string(1, c == ' ' ? '-' : c);
				text.color = screen.item == entry.cursor ? kCursorSlotColor : kSlotColor;
				break;
			}
			}
		});
	}
} // namespace game
