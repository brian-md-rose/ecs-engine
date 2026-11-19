#include "uidemo.hpp"

#include <array>
#include <string_view>

#include "assets.hpp"
#include "controls.hpp"
#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "graphics/animation.hpp"
#include "graphics/clip.hpp"
#include "graphics/color.hpp"
#include "graphics/font.hpp"
#include "graphics/screenspace.hpp"
#include "graphics/sprite.hpp"
#include "graphics/transform.hpp"
#include "input/input.hpp"
#include "ui/ninepatch.hpp"
#include "ui/ui.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::DespawnOnExit;
		using engine::ecs::Entity;
		using engine::ecs::NextState;
		using engine::ecs::Parent;
		using engine::ecs::Registry;
		using engine::ecs::World;
		using engine::graphics::Align;
		using engine::graphics::AlignedX;
		using engine::graphics::ClipRect;
		using engine::graphics::Color;
		using engine::graphics::LocalTransform;
		using engine::graphics::Position;
		using engine::graphics::ScreenSpace;
		using engine::graphics::Sprite;
		using engine::graphics::Text;
		using engine::graphics::Transform;
		using engine::ui::Hitbox;
		using engine::ui::Hovered;
		using engine::ui::Interactable;
		using engine::ui::NinePatch;
		using engine::ui::Pressed;
		using engine::ui::ScrollView;

		constexpr Screen kScope = Screen::UiDemo;

		constexpr float kPanelX = 12.0f;
		constexpr float kPanelY = 16.0f;
		constexpr int kPanelWidth = 200;
		constexpr int kPanelHeight = 256;
		constexpr int kBorder = 2; // of the panel and button pictures

		constexpr int kButtonWidth = 56;
		constexpr int kButtonHeight = 16;
		constexpr int kButtonGap = 8;
		constexpr float kButtonY = 52.0f;

		constexpr float kListX = 20.0f;
		constexpr float kListY = 78.0f;
		constexpr int kListWidth = 184;
		constexpr int kListHeight = 160;
		constexpr float kRowIndent = 4.0f;
		constexpr int kRowWrap = kListWidth - 2 * static_cast<int>(kRowIndent);
		constexpr int kRowGap = 4;

		constexpr Color kTitleColor{255, 200, 70};
		constexpr Color kTextColor{255, 255, 255};
		constexpr Color kHintColor{170, 215, 255};
		constexpr Color kListTint{150, 150, 150};
		constexpr Color kListFocusTint{225, 225, 255};

		constexpr std::array<std::string_view, kDemoButtons> kButtonLabels = {{"COUNT", "RESET", "BACK"}};

		// Everything the screen spawns is screen-space and goes when the state does.
		Entity SpawnScoped(Registry& registry, Position at)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, at);
			registry.Add<ScreenSpace>(entity);
			registry.Add<DespawnOnExit<Screen>>(entity, DespawnOnExit<Screen>{kScope});
			return entity;
		}

		Entity SpawnLabel(Registry& registry, std::string_view value, float centreX, float y, Color color)
		{
			const int width = static_cast<int>(value.size()) * kGlyphAdvance;
			const Entity entity = SpawnScoped(registry, {AlignedX(centreX, Align::Centre, width), y});
			registry.Add<Text>(entity, Text{.value = std::string(value), .color = color, .layer = kDemoTextLayer});
			return entity;
		}

		Entity SpawnPatch(Registry& registry, Position at, const engine::graphics::Clip& source, int width, int height,
		                  Color tint, std::uint8_t layer)
		{
			const Entity entity = SpawnScoped(registry, at);
			registry.Add<NinePatch>(entity, NinePatch{.source = source.firstFrame,
			                                          .left = kBorder,
			                                          .right = kBorder,
			                                          .top = kBorder,
			                                          .bottom = kBorder,
			                                          .width = width,
			                                          .height = height,
			                                          .tint = tint,
			                                          .layer = layer});
			return entity;
		}

		void SpawnButton(Registry& registry, const SpriteClips& clips, int index, std::string_view label)
		{
			const float x = kPanelX + 8.0f + static_cast<float>(index * (kButtonWidth + kButtonGap));
			const Entity button = SpawnPatch(registry, {x, kButtonY}, clips.button, kButtonWidth, kButtonHeight,
			                                 Color{255, 255, 255}, kDemoRowLayer);
			registry.Add<Hitbox>(button, Hitbox{.width = static_cast<float>(kButtonWidth),
			                                    .height = static_cast<float>(kButtonHeight),
			                                    .layer = kDemoRowLayer});
			registry.Add<Interactable>(button);
			registry.Add<engine::ui::FocusOrder>(button, engine::ui::FocusOrder{.order = index});
			registry.Add<DemoButton>(button, DemoButton{.action = static_cast<DemoAction>(index)});

			// The label is a child, so it moves with the button; it has no Hitbox, so the pointer
			// goes through it to the button.
			const float labelX = AlignedX(static_cast<float>(kButtonWidth) / 2.0f, Align::Centre,
			                              static_cast<int>(label.size()) * kGlyphAdvance);
			const Entity text = SpawnScoped(registry, {x + labelX, kButtonY + 4.0f});
			registry.Add<Parent>(text, Parent{button});
			registry.Add<LocalTransform>(text, LocalTransform{{labelX, 4.0f}});
			registry.Add<Text>(text, Text{.value = std::string(label), .color = kTextColor, .layer = kDemoTextLayer});
		}

		// The scrolling list: a view with a clip and a hitbox, a content child, and a text row per line.
		void SpawnList(Registry& registry)
		{
			engine::graphics::BitmapFont metrics;
			metrics.advance = kGlyphAdvance;
			metrics.lineHeight = kGlyphLineHeight;

			const Entity view = SpawnScoped(registry, {kListX, kListY});
			registry.Add<ClipRect>(view, ClipRect{.rect = {.x = static_cast<int>(kListX),
			                                               .y = static_cast<int>(kListY),
			                                               .width = kListWidth,
			                                               .height = kListHeight}});
			registry.Add<Hitbox>(view, Hitbox{.width = static_cast<float>(kListWidth),
			                                  .height = static_cast<float>(kListHeight),
			                                  .layer = kDemoListLayer});

			const Entity content = SpawnScoped(registry, {kListX, kListY});
			registry.Add<Parent>(content, Parent{view});
			registry.Add<LocalTransform>(content);

			float y = 0.0f;
			for (int row = 1; row <= kDemoRows; ++row)
			{
				const std::string value = DemoRowText(row);
				const Entity text = SpawnScoped(registry, {kListX + kRowIndent, kListY + y});
				registry.Add<Parent>(text, Parent{content});
				registry.Add<LocalTransform>(text, LocalTransform{{kRowIndent, y}});
				registry.Add<Text>(
				    text, Text{.value = value, .color = kTextColor, .layer = kDemoRowLayer, .wrapWidth = kRowWrap});
				y += static_cast<float>(engine::graphics::MeasureText(metrics, value, kRowWrap).height + kRowGap);
			}
			registry.Add<engine::ui::FocusOrder>(view, engine::ui::FocusOrder{.order = kDemoButtons});
			registry.Add<ScrollView>(
			    view,
			    ScrollView{.content = content, .viewHeight = static_cast<float>(kListHeight), .contentHeight = y});
		}
	} // namespace

	std::string DemoRowText(int row)
	{
		const std::string number = std::to_string(row);
		switch (row % 3)
		{
		case 1:
			return "ROW " + number + ": A SHORT ONE";
		case 2:
			return "ROW " + number + ": THIS LINE IS LONG ENOUGH THAT IT HAS TO WRAP";
		default:
			return "ROW " + number + ": THE LONGEST KIND, WHICH WRAPS ONTO A SECOND LINE AND THEN ONTO A THIRD ONE TOO";
		}
	}

	std::string DemoHintText(int line, engine::input::Device device, engine::input::PadFamily family)
	{
		using engine::input::PadButton;
		if (device != engine::input::Device::Pad) return line == 0 ? "WHEEL OR DRAG: SCROLL" : "ESC: BACK";
		if (line == 0)
		{
			return "D-PAD: MOVE   " + std::string(engine::input::PadButtonLabel(family, PadButton::South)) + ": PRESS";
		}
		return "R STICK: SCROLL   " + std::string(engine::input::PadButtonLabel(family, PadButton::Start)) + ": BACK";
	}

	std::string DemoCounterText(int clicks)
	{
		return "CLICKS: " + std::to_string(clicks);
	}

	void SpawnUiDemoScreen(World& world, const engine::ecs::FrameContext&)
	{
		Registry& registry = world.registry;
		const SpriteClips& clips = world.resources.Get<SpriteClips>();
		world.resources.Get<UiDemoState>() = UiDemoState{};
		const float centre = kPanelX + static_cast<float>(kPanelWidth) / 2.0f;

		const Entity panel = SpawnPatch(registry, {kPanelX, kPanelY}, clips.panel, kPanelWidth, kPanelHeight,
		                                Color{255, 255, 255}, kDemoPanelLayer);
		registry.Add<Hitbox>(panel, Hitbox{.width = static_cast<float>(kPanelWidth),
		                                   .height = static_cast<float>(kPanelHeight),
		                                   .layer = kDemoPanelLayer});

		SpawnLabel(registry, "UI DEMO", centre, kPanelY + 8.0f, kTitleColor);
		const Entity counter = SpawnLabel(registry, DemoCounterText(0), centre, kPanelY + 24.0f, kTextColor);
		registry.Add<DemoCounter>(counter);

		for (int i = 0; i < kDemoButtons; ++i)
		{
			SpawnButton(registry, clips, i, kButtonLabels[static_cast<std::size_t>(i)]);
		}

		const Entity frame = SpawnPatch(registry, {kListX - 4.0f, kListY - 4.0f}, clips.panel, kListWidth + 8,
		                                kListHeight + 8, kListTint, kDemoListLayer);
		registry.Add<DemoListFrame>(frame);
		SpawnList(registry);

		for (int line = 0; line < 2; ++line)
		{
			const Entity hint = SpawnLabel(registry, DemoHintText(line, engine::input::Device::Keyboard, {}), centre,
			                               kPanelY + 232.0f + static_cast<float>(line) * 10.0f, kHintColor);
			registry.Add<DemoHint>(hint, DemoHint{.line = line});
		}

		const Entity cursor = SpawnScoped(registry, {-32.0f, -32.0f});
		registry.Add<DemoCursor>(cursor);
		registry.Add<Sprite>(
		    cursor, Sprite{.source = clips.cursor.firstFrame, .tint = Color{255, 255, 255}, .layer = kDemoCursorLayer});
	}

	void UiDemoSystem(World& world, const engine::ecs::FrameContext&)
	{
		if (world.resources.Get<Controls>().WasPressed(Action::Pause))
		{
			world.resources.Get<NextState<Screen>>().Set(Screen::Options);
		}
	}

	void UiDemoLookSystem(World& world, const engine::ecs::FrameContext& context)
	{
		Registry& registry = world.registry;
		const SpriteClips& clips = world.resources.Get<SpriteClips>();
		const int clicks = world.resources.Get<UiDemoState>().clicks;

		registry.Each<DemoButton, NinePatch>([&](Entity entity, const DemoButton&, NinePatch& patch)
		{
			const bool pressed = registry.TryGetComponent<Pressed>(entity) != nullptr;
			// Focus (the pad's or Tab's) looks like hovering.
			const bool hovered = registry.TryGetComponent<Hovered>(entity) != nullptr ||
			                     registry.TryGetComponent<engine::ui::Focused>(entity) != nullptr;
			const engine::graphics::Clip& clip =
			    pressed ? clips.buttonPressed : (hovered ? clips.buttonHover : clips.button);
			patch.source = clip.firstFrame;
		});

		registry.Each<DemoCounter, Text>([&](Entity, const DemoCounter&, Text& text)
		{
			text.value = DemoCounterText(clicks);
		});

		bool listFocused = false;
		registry.Each<ScrollView>([&](Entity entity, const ScrollView&)
		{
			listFocused = listFocused || registry.TryGetComponent<engine::ui::Focused>(entity) != nullptr;
		});
		registry.Each<DemoListFrame, NinePatch>([&](Entity, const DemoListFrame&, NinePatch& patch)
		{
			patch.tint = listFocused ? kListFocusTint : kListTint;
		});

		if (context.input == nullptr) return;
		const engine::input::Input& in = *context.input;
		const float centre = kPanelX + static_cast<float>(kPanelWidth) / 2.0f;
		registry.Each<DemoHint, Text, Transform>([&](Entity, const DemoHint& hint, Text& text, Transform& transform)
		{
			text.value = DemoHintText(hint.line, in.lastUsed, in.PadFamilyInUse());
			const int width = static_cast<int>(text.value.size()) * kGlyphAdvance;
			transform.position.x = AlignedX(centre, Align::Centre, width);
		});

		const engine::input::Mouse& mouse = context.input->mouse;
		registry.Each<DemoCursor, Transform>([&](Entity, const DemoCursor&, Transform& transform)
		{
			// Off the screen when the pointer is outside the window, or the player has moved on to a pad.
			transform.position =
			    mouse.inside && in.lastUsed != engine::input::Device::Pad ? mouse.position : Position{-32.0f, -32.0f};
		});
	}

	void OnDemoClick(World& world, const engine::ui::Clicked& event)
	{
		if (event.button != engine::input::MouseButton::Left) return;
		if (!world.registry.IsAlive(event.entity)) return;
		const DemoButton* button = world.registry.TryGetComponent<DemoButton>(event.entity);
		if (button == nullptr) return;

		UiDemoState& state = world.resources.Get<UiDemoState>();
		switch (button->action)
		{
		case DemoAction::Count:
			++state.clicks;
			break;
		case DemoAction::Reset:
			state.clicks = 0;
			break;
		case DemoAction::Back:
			world.resources.Get<NextState<Screen>>().Set(Screen::Options);
			break;
		}
	}
} // namespace game
