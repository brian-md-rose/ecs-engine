#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "application/application.hpp"
#include "application/scheduledgame.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"
#include "editor/editor.hpp"
#include "editor/editorgame.hpp"
#include "editor/viewport.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/screenspace.hpp"
#include "graphics/font.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "ui/ninepatch.hpp"
#include "ui/ui.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::ecs::Registry;
	using engine::editor::EntityScreenRect;
	using engine::editor::PickEntity;
	using engine::graphics::Camera;
	using engine::graphics::Position;
	using engine::graphics::Shape;
	using engine::graphics::Transform;

	Entity AddBox(Registry& registry, float x, float y, float size, std::uint8_t layer = 0)
	{
		const Entity entity = registry.Create();
		registry.Add<Transform>(entity, Transform{{x, y}});
		registry.Add<Shape>(entity, Shape{.width = size, .height = size, .layer = layer});
		return entity;
	}

	Camera MakeCamera()
	{
		Camera camera;
		camera.logicalWidth = 320;
		camera.logicalHeight = 180;
		return camera;
	}
} // namespace

TEST_CASE("A world entity's rectangle goes through the camera, a screen-space one does not", "[editor][viewport]")
{
	Registry registry;
	Camera camera = MakeCamera();
	camera.position = {10, 0};
	camera.zoom = 2;

	const Entity world = AddBox(registry, 14, 3, 5);
	const auto rect = EntityScreenRect(registry, world, camera);
	REQUIRE(rect.has_value());
	CHECK(rect->x == 8.0F);
	CHECK(rect->y == 6.0F);
	CHECK(rect->width == 10.0F);

	const Entity hud = AddBox(registry, 14, 3, 5);
	registry.Add<engine::graphics::ScreenSpace>(hud);
	const Entity child = AddBox(registry, 1, 1, 4);
	registry.Add<engine::ecs::Parent>(child, engine::ecs::Parent{hud});
	CHECK(EntityScreenRect(registry, hud, camera)->x == 14.0F);
	CHECK(EntityScreenRect(registry, child, camera)->x == 1.0F); // screen space through its parent

	CHECK_FALSE(EntityScreenRect(registry, registry.Create(), camera).has_value());
}

TEST_CASE("Picking takes the highest layer, then the later entity", "[editor][viewport]")
{
	Registry registry;
	const Camera camera = MakeCamera();
	const Entity low = AddBox(registry, 0, 0, 20, 1);
	const Entity high = AddBox(registry, 10, 10, 20, 5);
	const Entity same = AddBox(registry, 12, 12, 4, 5);

	CHECK(PickEntity(registry, camera, Position{2, 2}) == low);
	CHECK(PickEntity(registry, camera, Position{15, 15}) == same);
	CHECK(PickEntity(registry, camera, Position{25, 25}) == high);
	CHECK(PickEntity(registry, camera, Position{100, 100}) == Entity::Null);
}

TEST_CASE("Zooming keeps the point under the pointer still", "[editor][viewport]")
{
	engine::editor::ViewTransform view;
	const Position origin{100, 50};
	const Position pointer{300, 200};
	const Position before = view.ToPicture(pointer, origin);
	view.ZoomAt(2.0F, pointer, origin);
	const Position after = view.ToPicture(pointer, origin);
	CHECK(view.zoom == 2.0F);
	CHECK(after.x == before.x);
	CHECK(after.y == before.y);

	view.ZoomAt(1000.0F, pointer, origin);
	CHECK(view.zoom == engine::editor::ViewTransform::kMaxZoom);
}

namespace
{
	class DrawingGame final : public engine::application::ScheduledGame
	{
	public:
		DrawingGame()
		{
			world.resources.Emplace<engine::graphics::Renderer2D>(MakeCamera());
			scheduler.Add(engine::ecs::Phase::Render, "draw",
			              [](engine::ecs::World& w, const engine::ecs::FrameContext& context)
			{
				auto& renderer2D = w.resources.Get<engine::graphics::Renderer2D>();
				context.renderer->Clear(0, 0, 0);
				renderer2D.Draw(*context.renderer, w.registry, 0.0F);
			});
		}
	};

	// Runs the editor game for a few frames.
	class Forwarding final : public engine::application::Game
	{
	public:
		explicit Forwarding(engine::editor::EditorGame& gameToRun) : inner(gameToRun) {}
		void Update(double dt, const engine::input::Input& input, engine::audio::AudioDevice* audio) override
		{
			++updates;
			inner.Update(dt, input, audio);
		}
		void Render(engine::graphics::Renderer& renderer, double alpha) override { inner.Render(renderer, alpha); }
		[[nodiscard]] bool WantsToQuit() const override { return updates >= 5; }

	private:
		engine::editor::EditorGame& inner;
		int updates = 0;
	};
} // namespace

TEST_CASE("The editor draws the game into a picture for the viewport", "[editor][viewport]")
{
	engine::application::ApplicationConfig config;
	config.title = "viewport test";
	config.vsync = false;
	config.frameCap = 0;
	engine::application::Application app(config);

	engine::editor::Editor editor;
	engine::editor::EditorGameConfig gameConfig;
	gameConfig.makeGame = []
	{
		return std::make_unique<DrawingGame>();
	};
	engine::editor::EditorGame game(editor, gameConfig);
	REQUIRE(game.IsValid());
	AddBox(game.EditWorld().registry, 10, 10, 20);
	app.SetEditorHost(&editor);

	Forwarding forwarding(game);
	if (app.Run(forwarding) != 0) SKIP("no offscreen video driver");
	CHECK(game.HasViewportPicture());
}

TEST_CASE("Moving an entity follows the camera zoom and the parent's local position", "[editor][viewport]")
{
	Registry registry;
	Camera camera = MakeCamera();
	camera.zoom = 2;

	const Entity box = AddBox(registry, 10, 10, 5);
	REQUIRE(engine::editor::MoveEntity(registry, box, camera, {8, -4}));
	CHECK(registry.TryGetComponent<Transform>(box)->position.x == 14.0F); // 8 screen pixels is 4 world units
	CHECK(registry.TryGetComponent<Transform>(box)->position.y == 8.0F);

	const Entity hud = AddBox(registry, 0, 0, 5);
	registry.Add<engine::graphics::ScreenSpace>(hud);
	REQUIRE(engine::editor::MoveEntity(registry, hud, camera, {8, 0}));
	CHECK(registry.TryGetComponent<Transform>(hud)->position.x == 8.0F);

	const Entity child = AddBox(registry, 50, 50, 5);
	registry.Add<engine::ecs::Parent>(child, engine::ecs::Parent{box});
	registry.Add<engine::graphics::LocalTransform>(child, engine::graphics::LocalTransform{{1, 1}});
	REQUIRE(engine::editor::MoveEntity(registry, child, camera, {2, 2}));
	CHECK(registry.TryGetComponent<engine::graphics::LocalTransform>(child)->position.x == 2.0F);
	CHECK(registry.TryGetComponent<Transform>(child)->position.x == 50.0F);

	CHECK_FALSE(engine::editor::MoveEntity(registry, Entity::Null, camera, {1, 1}));
}

TEST_CASE("PlayMouse maps the pointer into the picture and keeps a press until its release")
{
	using namespace engine;
	using editor::PointerSample;
	editor::PlayMouse play;
	editor::ViewTransform view;
	view.zoom = 2.0F;
	view.panX = 10.0F;
	const graphics::Position origin{100.0F, 50.0F};
	constexpr int kWidth = 64;
	constexpr int kHeight = 48;

	// Picture pixel (8, 4) is at window x = 100 + 10 + 8 * 2, y = 50 + 4 * 2.
	PointerSample sample;
	sample.window = {126.0F, 58.0F};
	sample.hovered = true;
	play.Feed(sample, view, origin, kWidth, kHeight);
	CHECK(play.mouse.inside);
	CHECK(play.mouse.position.x == 8.0F);
	CHECK(play.mouse.position.y == 4.0F);

	sample.down[0] = true;
	play.Feed(sample, view, origin, kWidth, kHeight);
	CHECK(play.mouse.WasPressed(input::MouseButton::Left));
	CHECK(play.mouse.IsDown(input::MouseButton::Left));
	play.mouse.EndUpdate();

	// The pointer leaves the viewport with the button held: the game still sees it, outside, then the release.
	sample.hovered = false;
	sample.window = {1000.0F, 58.0F};
	play.Feed(sample, view, origin, kWidth, kHeight);
	CHECK_FALSE(play.mouse.inside);
	CHECK(play.mouse.IsDown(input::MouseButton::Left));
	sample.down[0] = false;
	play.Feed(sample, view, origin, kWidth, kHeight);
	CHECK(play.mouse.WasReleased(input::MouseButton::Left));
	CHECK_FALSE(play.mouse.IsDown(input::MouseButton::Left));
	play.mouse.EndUpdate();
}

TEST_CASE("PlayMouse ignores a press that began outside the viewport, and the wheel outside it")
{
	using namespace engine;
	editor::PlayMouse play;
	const editor::ViewTransform view;

	editor::PointerSample sample;
	sample.hovered = false;
	sample.down[0] = true; // held while the pointer is over another panel
	sample.wheel = 2.0F;
	play.Feed(sample, view, {0, 0}, 64, 48);
	CHECK_FALSE(play.mouse.IsDown(input::MouseButton::Left));
	CHECK(play.mouse.wheel == 0.0F);

	// Moving onto the viewport with it still held is not a press.
	sample.hovered = true;
	play.Feed(sample, view, {0, 0}, 64, 48);
	CHECK_FALSE(play.mouse.IsDown(input::MouseButton::Left));
	CHECK(play.mouse.wheel == 2.0F);

	// Outside the picture but over the viewport: not "inside".
	sample.window = {100.0F, 10.0F};
	play.Feed(sample, view, {0, 0}, 64, 48);
	CHECK_FALSE(play.mouse.inside);

	play.Reset();
	CHECK(play.mouse.wheel == 0.0F);
}

TEST_CASE("PlayMouse: a button held when Play starts is not a press, and Reset releases what the game holds")
{
	using namespace engine;
	editor::PlayMouse play;
	const editor::ViewTransform view;

	editor::PointerSample sample;
	sample.hovered = true;
	sample.down[0] = true; // held since before the first frame (for example F5 pressed mid-drag)
	play.Feed(sample, view, {0, 0}, 64, 48);
	CHECK_FALSE(play.mouse.IsDown(input::MouseButton::Left));
	CHECK_FALSE(play.AnyOwned());

	sample.down[0] = false;
	play.Feed(sample, view, {0, 0}, 64, 48);
	sample.down[0] = true; // a new press is the game's
	play.Feed(sample, view, {0, 0}, 64, 48);
	CHECK(play.mouse.IsDown(input::MouseButton::Left));
	CHECK(play.AnyOwned());

	play.Reset();
	CHECK_FALSE(play.mouse.IsDown(input::MouseButton::Left));
	CHECK_FALSE(play.mouse.WasReleased(input::MouseButton::Left));
	CHECK_FALSE(play.AnyOwned());
	CHECK_FALSE(play.mouse.inside);
}

TEST_CASE("Text and nine-patch entities can be picked", "[editor][viewport]")
{
	Registry registry;
	const Camera camera = MakeCamera();

	const Entity label = registry.Create();
	registry.Add<Transform>(label, Transform{{20, 10}});
	registry.Add<engine::graphics::Text>(label, engine::graphics::Text{.value = "HELLO", .scale = 2});
	// Five letters, 6 pixels each, at scale 2: 60 wide; one line of 8, at scale 2: 16 tall.
	const auto textRect = EntityScreenRect(registry, label, camera);
	REQUIRE(textRect.has_value());
	CHECK(textRect->x == 20.0F);
	CHECK(textRect->width == 60.0F);
	CHECK(textRect->height == 16.0F);
	CHECK(PickEntity(registry, camera, {25, 15}) == label);
	CHECK(PickEntity(registry, camera, {90, 15}) == Entity::Null);

	// Centred text is drawn around its Transform, so its rectangle starts to the left of it.
	const Entity centred = registry.Create();
	registry.Add<Transform>(centred, Transform{{100, 50}});
	registry.Add<engine::graphics::Text>(centred,
	                                     engine::graphics::Text{.value = "AB", .align = engine::graphics::Align::Centre});
	CHECK(EntityScreenRect(registry, centred, camera)->x == 94.0F);

	const Entity panel = registry.Create();
	registry.Add<Transform>(panel, Transform{{200, 100}});
	registry.Add<engine::ui::NinePatch>(panel, engine::ui::NinePatch{.width = 40, .height = 30});
	CHECK(PickEntity(registry, camera, {210, 110}) == panel);
}
