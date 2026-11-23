#include "screens.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>

#include "assets.hpp"
#include "controls.hpp"
#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "gamestate.hpp"
#include "graphics/clip.hpp"
#include "graphics/color.hpp"
#include "graphics/font.hpp"
#include "graphics/screenspace.hpp"
#include "graphics/transform.hpp"
#include "hud.hpp"
#include "mazegame.hpp"
#include "menuui.hpp"
#include "nameentry.hpp"
#include "options.hpp"
#include "save.hpp"
#include "scenes.hpp"
#include "scoring.hpp"
#include "systems.hpp"
#include "ui/ninepatch.hpp"
#include "ui/ui.hpp"

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
		using engine::graphics::ClipRect;
		using engine::graphics::Color;
		using engine::graphics::LocalTransform;
		using engine::graphics::Position;
		using engine::graphics::ScreenSpace;
		using engine::graphics::Text;
		using engine::graphics::Transform;

		constexpr Color kTitleColor{255, 200, 70};
		constexpr Color kItemColor{255, 255, 255};
		constexpr Color kSlotColor{255, 255, 255};
		constexpr Color kCursorSlotColor{255, 200, 70};

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
		// The title, the menu, the hints and the high score (which ScreenTextSystem fills in, or hides before a
		// record).
		SpawnScene(world, "title", Screen::Title);
	}

	namespace
	{
		constexpr std::array<std::string_view, 24> kHelpLines = {{
		    "CONTROLS",
		    "ARROWS OR WASD: STEER THE PLAYER. IT KEEPS GOING UNTIL IT HITS A WALL, AND TURNS AT THE NEXT CORNER YOU "
		    "ASKED FOR.",
		    "D-PAD OR LEFT STICK: THE SAME ON A GAMEPAD.",
		    "P, ESC OR START: PAUSE.",
		    "ENTER, SPACE OR THE SOUTH BUTTON: CHOOSE.",
		    "",
		    "THE GOAL",
		    "EAT EVERY PELLET IN THE MAZE TO CLEAR THE LEVEL. A PELLET IS WORTH 5 POINTS.",
		    "A POWER PELLET IS WORTH 30 AND FRIGHTENS THE ENEMIES FOR A WHILE. EAT A FRIGHTENED ENEMY FOR BONUS "
		    "POINTS AND IT RUNS BACK HOME.",
		    "TOUCH AN ENEMY THAT ISN'T FRIGHTENED AND YOU LOSE A LIFE. LOSE THEM ALL AND THE GAME IS OVER.",
		    "",
		    "THE ENEMIES",
		    "EACH ENEMY HUNTS IN ITS OWN WAY: GRIT GOES STRAIGHT FOR YOU, SNARE WAITS AT THE NEXT JUNCTION ON YOUR "
		    "PATH, CLASP CLOSES IN FROM THE FAR SIDE, AND WISP FOLLOWS FROM AFAR BUT SLIPS AWAY WHEN YOU COME CLOSE.",
		    "THEY SWITCH BETWEEN HUNTING AND RETREATING TO THEIR CORNERS ON A TIMER.",
		    "",
		    "THE HIGH SCORE",
		    "BEAT THE RECORD AND YOU CAN ENTER YOUR NAME. IT IS SAVED, AND SHOWN ON THE TITLE SCREEN.",
		    "",
		    "IN THIS LIST",
		    "SCROLL WITH THE MOUSE WHEEL, BY DRAGGING, OR WITH THE RIGHT STICK.",
		    "CHOOSE BACK, OR PRESS ESC, TO RETURN TO THE TITLE.",
		    "",
		    "GOOD LUCK!",
		    "",
		}};

		constexpr int kHelpListWidth = 184; // as the list is in help.json; the rows wrap to it
		constexpr float kHelpIndent = 4.0f;
		constexpr int kHelpWrap = kHelpListWidth - 2 * static_cast<int>(kHelpIndent);
		constexpr int kHelpGap = 4;
	} // namespace

	std::span<const std::string_view> HelpLines() noexcept
	{
		return kHelpLines;
	}

	void SpawnHelpScreen(World& world, const engine::ecs::FrameContext&)
	{
		Registry& registry = world.registry;
		constexpr Screen kScope = Screen::Help;

		// The panel, title, list frame, BACK button and hint are in assets/scenes/help.json. The list's view and its
		// content are too, but its rows are spawned here: where each goes depends on how tall the ones before it wrap.
		const std::vector<Entity> entities = SpawnScene(world, "help", kScope);
		Entity view = Entity::Null;
		for (const Entity entity : entities)
		{
			if (registry.TryGetComponent<HelpList>(entity) != nullptr) view = entity;
		}
		const auto* scroll = view == Entity::Null ? nullptr : registry.TryGetComponent<engine::ui::ScrollView>(view);
		if (scroll == nullptr || !registry.IsAlive(scroll->content)) return; // the scene says why in the log
		const Entity content = scroll->content;
		const Position origin = registry.GetComponent<Transform>(content).position;
		registry.Add<engine::ui::Focused>(view); // so the right stick scrolls it

		engine::graphics::BitmapFont metrics;
		metrics.advance = kGlyphAdvance;
		metrics.lineHeight = kGlyphLineHeight;
		float y = 0.0f;
		for (const std::string_view line : HelpLines())
		{
			const std::string value(line);
			const Entity row = registry.Create();
			registry.Add<Transform>(row, Position{origin.x + kHelpIndent, origin.y + y});
			registry.Add<ScreenSpace>(row);
			registry.Add<DespawnOnExit<Screen>>(row, DespawnOnExit<Screen>{kScope});
			registry.Add<engine::ecs::Parent>(row, engine::ecs::Parent{content});
			registry.Add<LocalTransform>(row, LocalTransform{.position = {kHelpIndent, y}});
			const bool heading = !value.empty() && value.find(' ') == std::string::npos;
			registry.Add<Text>(row, Text{.value = value,
			                             .color = heading ? kTitleColor : kItemColor,
			                             .layer = kMenuTextLayer,
			                             .wrapWidth = kHelpWrap});
			y += static_cast<float>(engine::graphics::MeasureText(metrics, value, kHelpWrap).height + kHelpGap);
		}
		registry.GetComponent<engine::ui::ScrollView>(view).contentHeight = y;
	}

	void HelpSystem(World& world, const engine::ecs::FrameContext&)
	{
		const bool clicked = TakeMenuClick(world, MenuId::Help).has_value();
		if (clicked || Pad(world).WasPressed(Action::Pause) || Pad(world).WasPressed(Action::Confirm))
		{
			RequestScreen(world, Screen::Title);
		}
	}

	void SpawnOptionsScreen(World& world, const engine::ecs::FrameContext&)
	{
		world.resources.Get<MenuCursor>().options = 0;
		// The labels in the scene are placeholders: ScreenTextSystem writes each line from the settings.
		SpawnScene(world, "options", Screen::Options);
	}

	void SpawnNameEntryScreen(World& world, const engine::ecs::FrameContext&)
	{
		world.resources.Get<NameEntry>().cursor = 0;
		SpawnScene(world, "nameentry", Screen::NameEntry);
	}

	void SpawnLevelTransitionScreen(World& world, const engine::ecs::FrameContext&)
	{
		SpawnScene(world, "leveltransition", Screen::LevelTransition);
	}

	void SpawnPauseMenu(World& world, const engine::ecs::FrameContext&)
	{
		world.resources.Get<MenuCursor>().pause = 0;
		SpawnScene(world, "pause", Pause::Paused);
	}

	void TitleSystem(World& world, const engine::ecs::FrameContext&)
	{
		const Controls& controls = Pad(world);
		MenuCursor& cursor = world.resources.Get<MenuCursor>();
		cursor.title = MoveSelection(cursor.title, kTitleItems, controls.WasPressed(Action::MoveUp),
		                             controls.WasPressed(Action::MoveDown));
		const std::optional<std::size_t> clicked = TakeMenuClick(world, MenuId::Title);
		if (clicked) cursor.title = std::min(*clicked, kTitleItems - 1);
		if (!clicked && !controls.WasPressed(Action::Confirm)) return;

		switch (static_cast<TitleItem>(cursor.title))
		{
		case TitleItem::Start:
			StartNewGame(world);
			RequestScreen(world, Screen::Playing);
			break;
		case TitleItem::HowToPlay:
			RequestScreen(world, Screen::Help);
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
		// A click acts on its button as confirm does.
		const std::optional<std::size_t> clicked = TakeMenuClick(world, MenuId::Options);
		if (clicked) cursor.options = std::min(*clicked, kOptionItems - 1);
		const bool confirm = clicked.has_value() || controls.WasPressed(Action::Confirm);
		const auto item = static_cast<OptionItem>(cursor.options);

		if (item == OptionItem::Back)
		{
			if (confirm) RequestScreen(world, Screen::Title);
			return;
		}
		if (item == OptionItem::UiDemo)
		{
			if (confirm) RequestScreen(world, Screen::UiDemo);
			return;
		}
		const bool left = controls.WasPressed(Action::MoveLeft);
		const bool right = controls.WasPressed(Action::MoveRight) || confirm;
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
		const std::optional<std::size_t> clicked = TakeMenuClick(world, MenuId::Pause);
		if (clicked) cursor.pause = std::min(*clicked, kPauseItems - 1);
		if (!clicked && !controls.WasPressed(Action::Confirm)) return;
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
		const HighScore& high = world.resources.Get<HighScore>();
		const Score& score = world.resources.Get<Score>();
		const GameFlow& flow = world.resources.Get<GameFlow>();

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
			case TextKind::HelpBack:
				text.value = MenuLine(screen.label, true);
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
			case TextKind::HighScoreLabel:
				text.value = high.record > 0 ? screen.label : std::string();
				break;
			case TextKind::HighScoreValue:
				text.value = high.record > 0 ? (high.name.empty() ? "" : high.name + "  ") + std::to_string(high.record)
				                             : std::string();
				break;
			case TextKind::ScoreLine:
				text.value = screen.label + std::to_string(score.points);
				break;
			case TextKind::LevelLine:
				text.value = screen.label + std::to_string(flow.level);
				break;
			}
		});
	}
} // namespace game
