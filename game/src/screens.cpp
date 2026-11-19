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

		constexpr float kCentre = static_cast<float>(kLogicalWidth) / 2.0f;
		constexpr float kButtonPitch = 16.0f; // from one menu button to the next
		constexpr int kButtonHeight = 14;
		constexpr int kButtonPadding = 8; // between a button's edge and its text
		constexpr int kBorder = 2;        // of the panel and button pictures
		constexpr float kPanelMargin = 8.0f;
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
			registry.Add<Text>(
			    entity, Text{.value = std::string(value), .color = color, .layer = kMenuTextLayer, .scale = scale});
			registry.Add<DespawnOnExit<T>>(entity, DespawnOnExit<T>{scope});
			return entity;
		}

		template <typename T>
		void SpawnCentred(Registry& registry, T scope, std::string_view value, float y, Color color, int scale = 1)
		{
			SpawnText(registry, scope, value, {AlignedX(kCentre, Align::Centre, Width(value, scale)), y}, color, scale);
		}

		// A nine-patch of a picture, scoped to T, in screen space.
		template <typename T>
		Entity SpawnPatch(Registry& registry, T scope, const engine::graphics::Clip& source, Position at, int width,
		                  int height, std::uint8_t layer)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, at);
			registry.Add<ScreenSpace>(entity);
			registry.Add<DespawnOnExit<T>>(entity, DespawnOnExit<T>{scope});
			registry.Add<engine::ui::NinePatch>(entity, engine::ui::NinePatch{.source = source.firstFrame,
			                                                                  .left = kBorder,
			                                                                  .right = kBorder,
			                                                                  .top = kBorder,
			                                                                  .bottom = kBorder,
			                                                                  .width = width,
			                                                                  .height = height,
			                                                                  .layer = layer});
			return entity;
		}

		// A button of a menu: a nine-patch the pointer can hover and click, with its label as a child (so it
		// moves with it, and the pointer goes through the text to the button).
		template <typename T>
		void SpawnMenuButton(World& world, T scope, MenuId menu, TextKind kind, std::string_view label,
		                     std::size_t item, Position at, int width)
		{
			Registry& registry = world.registry;
			const SpriteClips& clips = world.resources.Get<SpriteClips>();
			const Entity button = SpawnPatch(registry, scope, clips.button, at, width, kButtonHeight, kMenuButtonLayer);
			registry.Add<engine::ui::Hitbox>(button, engine::ui::Hitbox{.width = static_cast<float>(width),
			                                                            .height = static_cast<float>(kButtonHeight),
			                                                            .layer = kMenuButtonLayer});
			registry.Add<engine::ui::Interactable>(button);
			registry.Add<engine::ui::FocusOrder>(button, engine::ui::FocusOrder{.order = static_cast<int>(item)});
			registry.Add<MenuButton>(button, MenuButton{.menu = menu, .item = item});

			const Position inside{static_cast<float>(kButtonPadding), 3.0f};
			const Entity text = registry.Create();
			registry.Add<Transform>(text, Position{at.x + inside.x, at.y + inside.y});
			registry.Add<ScreenSpace>(text);
			registry.Add<DespawnOnExit<T>>(text, DespawnOnExit<T>{scope});
			registry.Add<engine::ecs::Parent>(text, engine::ecs::Parent{button});
			registry.Add<LocalTransform>(text, LocalTransform{inside});
			registry.Add<Text>(text, Text{.value = std::string(label), .color = kItemColor, .layer = kMenuTextLayer});
			if (kind != TextKind::NameSlot)
			{
				registry.Add<ScreenText>(text, ScreenText{.kind = kind, .label = std::string(label), .item = item});
			}
		}

		// A menu: a panel with one button per item from y, in a column centred on the widest label (with its
		// cursor). Each button's text is a ScreenText of the given kind.
		template <typename T>
		void SpawnMenu(World& world, T scope, MenuId menu, TextKind kind, const std::string_view* labels,
		               std::size_t count, float y)
		{
			std::size_t widest = 0;
			for (std::size_t i = 0; i < count; ++i)
			{
				widest = std::max(widest, MenuLine(labels[i], true).size());
			}
			const int width = Width(std::string(widest, ' ')) + 2 * kButtonPadding;
			const float left = AlignedX(kCentre, Align::Centre, width);
			const float height = static_cast<float>(count) * kButtonPitch - (kButtonPitch - kButtonHeight);
			SpawnPatch(world.registry, scope, world.resources.Get<SpriteClips>().panel,
			           {left - kPanelMargin, y - kPanelMargin}, width + 2 * static_cast<int>(kPanelMargin),
			           static_cast<int>(height + 2.0f * kPanelMargin), kMenuLayer);
			for (std::size_t i = 0; i < count; ++i)
			{
				SpawnMenuButton(world, scope, menu, kind, labels[i], i,
				                {left, y + static_cast<float>(i) * kButtonPitch}, width);
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
		constexpr std::string_view kLabels[] = {"START", "HOW TO PLAY", "OPTIONS", "QUIT"};
		static_assert(std::size(kLabels) == kTitleItems);
		SpawnMenu(world, kScope, MenuId::Title, TextKind::TitleItem, kLabels, std::size(kLabels), 112.0f);

		// Whoever holds the record, once there is one.
		const HighScore& high = world.resources.Get<HighScore>();
		if (high.record > 0)
		{
			SpawnCentred(registry, kScope, "HIGH SCORE", 192.0f, kHintColor);
			const std::string holder = (high.name.empty() ? "" : high.name + "  ") + std::to_string(high.record);
			SpawnCentred(registry, kScope, holder, 204.0f, kItemColor);
		}

		SpawnCentred(registry, kScope, "MOUSE, ARROWS OR D-PAD: CHOOSE", 232.0f, kHintColor);
		SpawnCentred(registry, kScope, "CLICK, ENTER OR SPACE: SELECT", 244.0f, kHintColor);
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

		constexpr float kHelpPanelX = 12.0f;
		constexpr float kHelpPanelY = 40.0f;
		constexpr int kHelpPanelWidth = 200;
		constexpr int kHelpPanelHeight = 188;
		constexpr float kHelpListX = 20.0f;
		constexpr float kHelpListY = 48.0f;
		constexpr int kHelpListWidth = 184;
		constexpr int kHelpListHeight = 172;
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
		SpawnCentred(registry, kScope, "HOW TO PLAY", 16.0f, kTitleColor, 2);
		SpawnPatch(registry, kScope, world.resources.Get<SpriteClips>().panel, {kHelpPanelX, kHelpPanelY},
		           kHelpPanelWidth, kHelpPanelHeight, kMenuLayer);

		const auto scoped = [&](Position at)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, at);
			registry.Add<ScreenSpace>(entity);
			registry.Add<DespawnOnExit<Screen>>(entity, DespawnOnExit<Screen>{kScope});
			return entity;
		};

		// The scrolling list: a view with a clip and a hitbox, a content child, and a text row per line.
		engine::graphics::BitmapFont metrics;
		metrics.advance = kGlyphAdvance;
		metrics.lineHeight = kGlyphLineHeight;
		const Entity view = scoped({kHelpListX, kHelpListY});
		registry.Add<ClipRect>(view, ClipRect{.rect = {.x = static_cast<int>(kHelpListX),
		                                               .y = static_cast<int>(kHelpListY),
		                                               .width = kHelpListWidth,
		                                               .height = kHelpListHeight}});
		registry.Add<engine::ui::Hitbox>(view, engine::ui::Hitbox{.width = static_cast<float>(kHelpListWidth),
		                                                          .height = static_cast<float>(kHelpListHeight),
		                                                          .layer = kMenuButtonLayer});
		registry.Add<HelpList>(view);
		registry.Add<engine::ui::Focused>(view); // so the right stick scrolls it

		const Entity content = scoped({kHelpListX, kHelpListY});
		registry.Add<engine::ecs::Parent>(content, engine::ecs::Parent{view});
		registry.Add<LocalTransform>(content);

		float y = 0.0f;
		for (const std::string_view line : HelpLines())
		{
			const std::string value(line);
			const Entity row = scoped({kHelpListX + kHelpIndent, kHelpListY + y});
			registry.Add<engine::ecs::Parent>(row, engine::ecs::Parent{content});
			registry.Add<LocalTransform>(row, LocalTransform{{kHelpIndent, y}});
			const bool heading = !value.empty() && value.find(' ') == std::string::npos;
			registry.Add<Text>(row, Text{.value = value,
			                             .color = heading ? kTitleColor : kItemColor,
			                             .layer = kMenuTextLayer,
			                             .wrapWidth = kHelpWrap});
			y += static_cast<float>(engine::graphics::MeasureText(metrics, value, kHelpWrap).height + kHelpGap);
		}
		registry.Add<engine::ui::ScrollView>(
		    view, engine::ui::ScrollView{
		              .content = content, .viewHeight = static_cast<float>(kHelpListHeight), .contentHeight = y});

		const int width = Width("> BACK") + 2 * kButtonPadding;
		SpawnMenuButton(world, kScope, MenuId::Help, TextKind::HelpBack, "BACK", 0,
		                {AlignedX(kCentre, Align::Centre, width), 238.0f}, width);
		SpawnCentred(registry, kScope, "WHEEL OR R STICK: SCROLL", 264.0f, kHintColor);
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
		Registry& registry = world.registry;
		constexpr Screen kScope = Screen::Options;
		world.resources.Get<MenuCursor>().options = 0;
		SpawnCentred(registry, kScope, "OPTIONS", 40.0f, kTitleColor, 2);

		// The labels are placeholders: ScreenTextSystem writes each line from the settings.
		constexpr std::string_view kLabels[] = {"SOUND: OFF",        "MUSIC: OFF",       "VOLUME: 10", "LIVES: 3",
		                                        "DISPLAY: WINDOWED", "SCALING: INTEGER", "UI DEMO",    "BACK"};
		static_assert(std::size(kLabels) == kOptionItems);
		SpawnMenu(world, kScope, MenuId::Options, TextKind::OptionItem, kLabels, std::size(kLabels), 80.0f);

		SpawnCentred(registry, kScope, "MOUSE, ARROWS OR D-PAD: CHOOSE", 224.0f, kHintColor);
		SpawnCentred(registry, kScope, "LEFT RIGHT, CLICK OR ENTER: CHANGE", 236.0f, kHintColor);
		SpawnCentred(registry, kScope, "ESC: BACK", 248.0f, kHintColor);
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
		SpawnMenu(world, kScope, MenuId::Pause, TextKind::PauseItem, kLabels, std::size(kLabels), 144.0f);
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
			}
		});
	}
} // namespace game
