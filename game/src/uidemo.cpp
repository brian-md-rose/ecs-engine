#include "uidemo.hpp"

#include <array>
#include <string_view>

#include "assets.hpp"
#include "controls.hpp"
#include "core/log.hpp"
#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/scene.hpp"
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

		// The layout (positions, sizes, colours, labels) is in assets/scenes/uidemo.json; these are what the code
		// still needs: the panel's centre for the hints, and the list rows it generates.
		constexpr float kPanelX = 12.0f;
		constexpr int kPanelWidth = 200;

		constexpr float kListX = 20.0f;
		constexpr float kListY = 78.0f;
		constexpr int kListWidth = 184;
		constexpr float kRowIndent = 4.0f;
		constexpr int kRowWrap = kListWidth - 2 * static_cast<int>(kRowIndent);
		constexpr int kRowGap = 4;

		constexpr Color kTextColor{255, 255, 255};
		constexpr Color kListTint{150, 150, 150};
		constexpr Color kListFocusTint{225, 225, 255};

		// Everything the screen spawns is screen-space and goes when the state does.
		Entity SpawnScoped(Registry& registry, Position at)
		{
			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, at);
			registry.Add<ScreenSpace>(entity);
			registry.Add<DespawnOnExit<Screen>>(entity, DespawnOnExit<Screen>{kScope});
			return entity;
		}

		// The text rows of the scrolling list. The view and its content child are in the scene file; the rows are
		// generated, because where each goes depends on how tall the ones before it wrap.
		void SpawnListRows(Registry& registry)
		{
			engine::graphics::BitmapFont metrics;
			metrics.advance = kGlyphAdvance;
			metrics.lineHeight = kGlyphLineHeight;

			// Found first, then spawned into: adding entities inside Each is not allowed.
			Entity view = Entity::Null;
			registry.Each<ScrollView>([&view](Entity entity, const ScrollView&)
			{
				view = entity;
			});
			if (view.IsNull()) return;
			const Entity content = registry.GetComponent<ScrollView>(view).content;
			if (!registry.IsAlive(content)) return;
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
			registry.GetComponent<ScrollView>(view).contentHeight = y;
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
		world.resources.Get<UiDemoState>() = UiDemoState{};

		// The layout is a scene file (assets/scenes/uidemo.json). A broken one leaves the screen empty, and says why.
		const engine::ecs::SceneLoadResult loaded = LoadScene(
		    registry, world.resources.Get<engine::ecs::TypeRegistry>(), world.resources.Get<UiDemoScene>().json);
		for (const engine::ecs::Issue& issue : loaded.issues.errors)
		{
			engine::core::LogError("uidemo scene: " + issue.path + ": " + issue.message);
		}
		if (!loaded.Ok()) return;

		// Everything the screen spawns goes when the state does.
		for (const Entity entity : loaded.entities)
		{
			registry.Add<DespawnOnExit<Screen>>(entity, DespawnOnExit<Screen>{kScope});
		}
		ResolveClipNames(registry, world.resources.Get<SpriteClips>());
		SpawnListRows(registry);
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

namespace game
{
	void RegisterUiDemoComponents(engine::ecs::TypeRegistry& types)
	{
		types.Register<DemoButton>("DemoButton")
		    .EnumField("action", &DemoButton::action,
		               {{"Count", DemoAction::Count}, {"Reset", DemoAction::Reset}, {"Back", DemoAction::Back}});
		types.RegisterTag<DemoCounter>("DemoCounter");
		types.RegisterTag<DemoCursor>("DemoCursor");
		types.RegisterTag<DemoListFrame>("DemoListFrame");
		types.Register<DemoHint>("DemoHint").Field("line", &DemoHint::line);
		types.Register<ClipName>("ClipName").Field("value", &ClipName::value);
	}

	void ResolveClipNames(engine::ecs::Registry& registry, const SpriteClips& clips)
	{
		const auto find = [&clips](const std::string& name) -> const engine::graphics::Clip*
		{
			if (name == "panel") return &clips.panel;
			if (name == "button") return &clips.button;
			if (name == "buttonHover") return &clips.buttonHover;
			if (name == "buttonPressed") return &clips.buttonPressed;
			if (name == "cursor") return &clips.cursor;
			return nullptr;
		};
		registry.Each<ClipName, engine::ui::NinePatch>(
		    [&](engine::ecs::Entity, const ClipName& clip, engine::ui::NinePatch& patch)
		{
			if (const engine::graphics::Clip* found = find(clip.value)) patch.source = found->firstFrame;
		});
		registry.Each<ClipName, engine::graphics::Sprite>(
		    [&](engine::ecs::Entity, const ClipName& clip, engine::graphics::Sprite& sprite)
		{
			if (const engine::graphics::Clip* found = find(clip.value)) sprite.source = found->firstFrame;
		});
	}
} // namespace game
