#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include "application/application.hpp"
#include "application/scheduledgame.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"
#include "editor/editor.hpp"
#include "editor/editorgame.hpp"
#include "editor/viewport.hpp"
#include "graphics/font.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/screenspace.hpp"
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
		registry.Add<Transform>(entity, Transform{.position = {x, y}});
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
	registry.Add<engine::graphics::LocalTransform>(child, engine::graphics::LocalTransform{.position = {1, 1}});
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
	registry.Add<Transform>(label, Transform{.position = {20, 10}});
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
	registry.Add<Transform>(centred, Transform{.position = {100, 50}});
	registry.Add<engine::graphics::Text>(
	    centred, engine::graphics::Text{.value = "AB", .align = engine::graphics::Align::Centre});
	CHECK(EntityScreenRect(registry, centred, camera)->x == 94.0F);

	const Entity panel = registry.Create();
	registry.Add<Transform>(panel, Transform{.position = {200, 100}});
	registry.Add<engine::ui::NinePatch>(panel, engine::ui::NinePatch{.width = 40, .height = 30});
	CHECK(PickEntity(registry, camera, {210, 110}) == panel);
}

TEST_CASE("ScrollIntoView centres an off-screen rectangle and leaves a visible one", "[editor][viewport]")
{
	engine::editor::ViewTransform view;
	view.zoom = 2.0F;
	view.panX = 10.0F;
	view.panY = 0.0F;
	const float width = 400;
	const float height = 300;

	// Window x of this rectangle is 10 + 50 * 2 = 110 to 150: showing.
	CHECK_FALSE(view.ScrollIntoView({50, 50, 20, 20}, width, height));
	CHECK(view.panX == 10.0F);

	// Window x of 10 + 600 * 2 is far past the right edge: it is centred.
	CHECK(view.ScrollIntoView({600, 40, 20, 20}, width, height));
	CHECK(view.panX == 200.0F - 610.0F * 2.0F);
	CHECK(view.panY == 150.0F - 50.0F * 2.0F);
	CHECK_FALSE(view.ScrollIntoView({600, 40, 20, 20}, width, height)); // now showing

	// Partly showing counts as showing.
	view = {};
	CHECK_FALSE(view.ScrollIntoView({390, 10, 50, 10}, width, height));
}

TEST_CASE("DragTarget adds the movement, locks to one axis and snaps to a grid", "[editor][viewport]")
{
	using engine::editor::DragTarget;
	const Position start{10, 10};

	const Position free = DragTarget(start, {5, -3}, false, 0);
	CHECK(free.x == 15.0F);
	CHECK(free.y == 7.0F);

	const Position horizontal = DragTarget(start, {5, -3}, true, 0);
	CHECK(horizontal.x == 15.0F);
	CHECK(horizontal.y == 10.0F);
	const Position vertical = DragTarget(start, {2, -9}, true, 0);
	CHECK(vertical.x == 10.0F);
	CHECK(vertical.y == 1.0F);

	const Position snapped = DragTarget(start, {5, -3}, false, 8);
	CHECK(snapped.x == 16.0F); // 15 rounds to 16
	CHECK(snapped.y == 8.0F);  // 7 rounds to 8
	const Position both = DragTarget({0, 0}, {13, 2}, true, 8);
	CHECK(both.x == 16.0F);
	CHECK(both.y == 0.0F);
}

TEST_CASE("MovablePosition is the LocalTransform of a child and the Transform of anything else", "[editor][viewport]")
{
	using engine::editor::MovablePosition;
	using engine::editor::SetMovablePosition;
	using engine::editor::UnitsPerPixel;
	Registry registry;
	Camera camera = MakeCamera();
	camera.zoom = 2;

	const Entity plain = AddBox(registry, 4, 6, 5);
	CHECK(MovablePosition(registry, plain)->x == 4.0F);
	REQUIRE(SetMovablePosition(registry, plain, {9, 1}));
	CHECK(registry.GetComponent<Transform>(plain).position.x == 9.0F);
	CHECK(UnitsPerPixel(registry, plain, camera) == 0.5F);

	const Entity parent = AddBox(registry, 100, 100, 5);
	const Entity child = AddBox(registry, 100, 100, 5);
	registry.Add<engine::ecs::Parent>(child, engine::ecs::Parent{parent});
	registry.Add<engine::graphics::LocalTransform>(child, engine::graphics::LocalTransform{.position = {3, 4}});
	CHECK(MovablePosition(registry, child)->x == 3.0F);
	REQUIRE(SetMovablePosition(registry, child, {7, 8}));
	CHECK(registry.GetComponent<engine::graphics::LocalTransform>(child).position.y == 8.0F);
	CHECK(registry.GetComponent<Transform>(child).position.x == 100.0F);

	registry.Add<engine::graphics::ScreenSpace>(plain);
	CHECK(UnitsPerPixel(registry, plain, camera) == 1.0F);
	CHECK_FALSE(MovablePosition(registry, registry.Create()).has_value());
}

TEST_CASE("The translate gizmo picks its handles and lets only an axis through", "[editor][viewport][gizmo]")
{
	using engine::editor::GizmoHandle;
	using engine::editor::GizmoMove;
	using engine::editor::PickGizmo;
	const Position pivot{100, 100};

	CHECK(PickGizmo(pivot, {102, 98}) == GizmoHandle::Free);
	CHECK(PickGizmo(pivot, {140, 101}) == GizmoHandle::X);
	CHECK(PickGizmo(pivot, {99, 140}) == GizmoHandle::Y);
	CHECK(PickGizmo(pivot, {60, 100}) == GizmoHandle::None); // the arrows point right and down only
	CHECK(PickGizmo(pivot, {140, 120}) == GizmoHandle::None);
	CHECK(PickGizmo(pivot, {300, 100}) == GizmoHandle::None);

	CHECK(GizmoMove(GizmoHandle::X, {5, 7}).y == 0.0F);
	CHECK(GizmoMove(GizmoHandle::X, {5, 7}).x == 5.0F);
	CHECK(GizmoMove(GizmoHandle::Y, {5, 7}).x == 0.0F);
	CHECK(GizmoMove(GizmoHandle::Y, {5, 7}).y == 7.0F);
	CHECK(GizmoMove(GizmoHandle::Free, {5, 7}).x == 5.0F);
	CHECK(GizmoMove(GizmoHandle::None, {5, 7}).y == 0.0F);
}

TEST_CASE("The gizmo sits at the mean middle of what is drawn", "[editor][viewport][gizmo]")
{
	Registry registry;
	const Camera camera = MakeCamera();
	const Entity a = AddBox(registry, 0, 0, 10);
	const Entity b = AddBox(registry, 20, 40, 10);
	const Entity bare = registry.Create();

	const auto one = engine::editor::SelectionCentre(registry, {a}, camera);
	REQUIRE(one.has_value());
	const auto both = engine::editor::SelectionCentre(registry, {a, b, bare}, camera);
	REQUIRE(both.has_value());
	const auto ra = EntityScreenRect(registry, a, camera);
	const auto rb = EntityScreenRect(registry, b, camera);
	REQUIRE(ra.has_value());
	REQUIRE(rb.has_value());
	CHECK(both->x == ((ra->x + ra->width / 2.0F) + (rb->x + rb->width / 2.0F)) / 2.0F);
	CHECK(both->y == ((ra->y + ra->height / 2.0F) + (rb->y + rb->height / 2.0F)) / 2.0F);
	CHECK_FALSE(engine::editor::SelectionCentre(registry, {bare}, camera).has_value());
}

namespace
{
	Entity AddTurnedBox(Registry& registry, float x, float y, float width, float height, float rotation,
	                    engine::graphics::Scale scale = {})
	{
		const Entity entity = registry.Create();
		registry.Add<Transform>(entity, Transform{.position = {x, y}, .rotation = rotation, .scale = scale});
		registry.Add<Shape>(entity, Shape{.width = width, .height = height});
		return entity;
	}
} // namespace

TEST_CASE("A turned and stretched box has its corners turned about its middle", "[editor][viewport][rotation]")
{
	using engine::editor::EntityScreenCorners;
	Registry registry;
	const Camera camera = MakeCamera();

	// A 40 by 10 box turned a quarter clockwise stands upright about its middle (20, 5).
	const Entity upright = AddTurnedBox(registry, 0, 0, 40, 10, 90);
	const auto corners = EntityScreenCorners(registry, upright, camera);
	REQUIRE(corners.has_value());
	CHECK((*corners)[0].x == Catch::Approx(25.0F));
	CHECK((*corners)[0].y == Catch::Approx(-15.0F));
	CHECK((*corners)[2].x == Catch::Approx(15.0F));
	CHECK((*corners)[2].y == Catch::Approx(25.0F));

	// Doubled it is twice as far from the middle, and the unrotated rectangle is still the box before it was turned.
	const Entity big = AddTurnedBox(registry, 0, 0, 10, 10, 0, {2, 2});
	const auto doubled = EntityScreenCorners(registry, big, camera);
	REQUIRE(doubled.has_value());
	CHECK((*doubled)[0].x == Catch::Approx(-5.0F));
	CHECK((*doubled)[2].x == Catch::Approx(15.0F));
	CHECK(EntityScreenRect(registry, big, camera)->width == 10.0F);

	// A plain box has the rectangle's own corners, and a box with nothing drawn has none.
	const Entity plain = AddBox(registry, 3, 4, 5);
	const auto flat = EntityScreenCorners(registry, plain, camera);
	REQUIRE(flat.has_value());
	CHECK((*flat)[0].x == 3.0F);
	CHECK((*flat)[2].y == 9.0F);
	CHECK_FALSE(EntityScreenCorners(registry, registry.Create(), camera).has_value());
}

TEST_CASE("Picking and the box select follow a turned and stretched entity", "[editor][viewport][rotation]")
{
	Registry registry;
	const Camera camera = MakeCamera();
	// The box stands upright after the turn: x from 15 to 25, y from -15 to 25.
	const Entity upright = AddTurnedBox(registry, 0, 0, 40, 10, 90);

	CHECK(PickEntity(registry, camera, {20, -10}) == upright); // only the turned box is there
	CHECK(PickEntity(registry, camera, {20, 20}) == upright);
	CHECK(PickEntity(registry, camera, {5, 5}) == Entity::Null); // the unturned box was there
	CHECK(PickEntity(registry, camera, {35, 5}) == Entity::Null);

	CHECK(engine::editor::EntitiesInBox(registry, camera, {16, -16, 4, 4}) == std::vector<Entity>{upright});
	CHECK(engine::editor::EntitiesInBox(registry, camera, {0, 0, 10, 10}).empty());
	// A box that crosses the turned outline's edge but holds no corner of either shape still counts.
	CHECK(engine::editor::EntitiesInBox(registry, camera, {10, 4, 20, 2}) == std::vector<Entity>{upright});

	// A turn of 45 degrees makes a diamond: its corner reaches out, its box corner does not.
	const Entity diamond = AddTurnedBox(registry, 100, 0, 20, 20, 45);
	CHECK(PickEntity(registry, camera, {110, -3}) == diamond);     // above the original top edge
	CHECK(PickEntity(registry, camera, {101, 1}) == Entity::Null); // the original corner is now empty
	// Stretched: scale 2 doubles it about the middle, and a negative scale still has its body.
	const Entity wide = AddTurnedBox(registry, 200, 0, 10, 10, 0, {-3, 1});
	CHECK(PickEntity(registry, camera, {190, 5}) == wide);
	CHECK(PickEntity(registry, camera, {214, 5}) == wide);
	CHECK(PickEntity(registry, camera, {221, 5}) == Entity::Null);
}

TEST_CASE("A child of a turned parent moves in the parent's axes", "[editor][viewport][rotation]")
{
	using engine::graphics::LocalTransform;
	Registry registry;
	const Camera camera = MakeCamera();
	const Entity parent = AddTurnedBox(registry, 0, 0, 10, 10, 90, {2, 1});
	const Entity child = AddBox(registry, 0, 0, 4);
	registry.Add<engine::ecs::Parent>(child, engine::ecs::Parent{parent});
	registry.Add<LocalTransform>(child, LocalTransform{});

	// To go down the screen by 4 the child goes 4 along the parent's x (which points down after the turn), and the
	// parent's scale of 2 along that axis halves it.
	const Position local = engine::editor::LocalDelta(registry, child, {0, 4});
	CHECK(local.x == Catch::Approx(2.0F));
	CHECK(local.y == Catch::Approx(0.0F).margin(1e-5));
	// Where it ends up agrees: the world movement is the local one turned and stretched.
	REQUIRE(engine::editor::MoveEntity(registry, child, camera, {0, 4}));
	CHECK(registry.TryGetComponent<LocalTransform>(child)->position.x == Catch::Approx(2.0F));

	// Anything without a turned parent is moved as it was.
	const Entity free = AddBox(registry, 0, 0, 4);
	CHECK(engine::editor::LocalDelta(registry, free, {3, 4}).x == 3.0F);
	CHECK(engine::editor::LocalDelta(registry, Entity::Null, {3, 4}).y == 4.0F);
}

TEST_CASE("MovableRotation and MovableScale are on the LocalTransform of a child, else the Transform",
          "[editor][viewport][rotation]")
{
	using engine::graphics::LocalTransform;
	using engine::graphics::Scale;
	Registry registry;
	const Entity alone = AddTurnedBox(registry, 0, 0, 4, 4, 30, {2, 3});
	CHECK(engine::editor::MovableRotation(registry, alone).value() == 30.0F);
	CHECK(engine::editor::MovableScale(registry, alone)->y == 3.0F);
	REQUIRE(engine::editor::SetMovableRotation(registry, alone, 80));
	REQUIRE(engine::editor::SetMovableScale(registry, alone, Scale{4, 5}));
	CHECK(registry.TryGetComponent<Transform>(alone)->rotation == 80.0F);
	CHECK(registry.TryGetComponent<Transform>(alone)->scale.x == 4.0F);

	const Entity child = AddTurnedBox(registry, 0, 0, 4, 4, 10);
	registry.Add<engine::ecs::Parent>(child, engine::ecs::Parent{alone});
	registry.Add<LocalTransform>(child, LocalTransform{.rotation = 5, .scale = {1, 2}});
	CHECK(engine::editor::MovableRotation(registry, child).value() == 5.0F);
	REQUIRE(engine::editor::SetMovableScale(registry, child, Scale{7, 7}));
	CHECK(registry.TryGetComponent<LocalTransform>(child)->scale.x == 7.0F);
	CHECK(registry.TryGetComponent<Transform>(child)->scale.x == 1.0F);

	CHECK_FALSE(engine::editor::MovableRotation(registry, registry.Create()).has_value());
	CHECK_FALSE(engine::editor::SetMovableScale(registry, Entity::Null, Scale{}));
}

TEST_CASE("The rotate gizmo is a ring and turns by the angle the pointer goes round", "[editor][viewport][gizmo]")
{
	using engine::editor::AngleBetween;
	using engine::editor::GizmoHandle;
	using engine::editor::PickRotateGizmo;
	using engine::editor::TurnedRotation;
	const Position pivot{100, 100};

	CHECK(PickRotateGizmo(pivot, {148, 100}) == GizmoHandle::Ring);
	CHECK(PickRotateGizmo(pivot, {100, 52}) == GizmoHandle::Ring);
	CHECK(PickRotateGizmo(pivot, {100, 100}) == GizmoHandle::None); // inside the ring
	CHECK(PickRotateGizmo(pivot, {160, 100}) == GizmoHandle::None);

	// Clockwise on screen is positive: from the right of the pivot to below it is a quarter turn.
	CHECK(AngleBetween(pivot, {150, 100}, {100, 150}) == Catch::Approx(90.0F));
	CHECK(AngleBetween(pivot, {100, 150}, {150, 100}) == Catch::Approx(-90.0F));
	// Crossing the left of the pivot is a small step, not a jump of nearly 360.
	CHECK(AngleBetween(pivot, {50, 99}, {50, 101}) == Catch::Approx(-2.29F).margin(0.05));

	CHECK(TurnedRotation(10, 20, 0) == Catch::Approx(30.0F));
	CHECK(TurnedRotation(170, 20, 0) == Catch::Approx(-170.0F)); // wraps
	CHECK(TurnedRotation(0, 100, 15) == Catch::Approx(105.0F));  // rounds to a multiple of 15
	CHECK(TurnedRotation(0, 106, 15) == Catch::Approx(105.0F));
	CHECK(TurnedRotation(0, 180, 0) == Catch::Approx(180.0F));
	CHECK(TurnedRotation(0, -180, 0) == Catch::Approx(180.0F));
}

TEST_CASE("The scale gizmo turns with the entity and scales by the travel along an arrow", "[editor][viewport][gizmo]")
{
	using engine::editor::GizmoHandle;
	using engine::editor::PickGizmo;
	using engine::editor::ScaledValue;
	using engine::editor::ScaleFactor;
	const Position pivot{100, 100};

	// Turned a quarter, the X arrow points down and the Y arrow points left.
	CHECK(PickGizmo(pivot, {100, 140}, {}, 90) == GizmoHandle::X);
	CHECK(PickGizmo(pivot, {60, 100}, {}, 90) == GizmoHandle::Y);
	CHECK(PickGizmo(pivot, {140, 100}, {}, 90) == GizmoHandle::None);
	CHECK(PickGizmo(pivot, {102, 98}, {}, 90) == GizmoHandle::Free);

	CHECK(ScaleFactor(GizmoHandle::X, 0, {56, 30}) == Catch::Approx(2.0F)); // one arrow length more
	CHECK(ScaleFactor(GizmoHandle::X, 0, {-28, 30}) == Catch::Approx(0.5F));
	CHECK(ScaleFactor(GizmoHandle::Y, 0, {30, 56}) == Catch::Approx(2.0F));
	CHECK(ScaleFactor(GizmoHandle::X, 90, {30, 56}) == Catch::Approx(2.0F)); // the arrow points down now
	CHECK(ScaleFactor(GizmoHandle::Free, 0, {56, -56}) == Catch::Approx(2.0F));
	CHECK(ScaleFactor(GizmoHandle::Free, 0, {-56, 56}) == Catch::Approx(0.0F));
	CHECK(ScaleFactor(GizmoHandle::None, 0, {56, 56}) == 1.0F);
	CHECK(ScaleFactor(GizmoHandle::Ring, 0, {56, 56}) == 1.0F);

	CHECK(ScaledValue(2, 1.5F, 0) == Catch::Approx(3.0F));
	CHECK(ScaledValue(1, 1.6F, 0.25F) == Catch::Approx(1.5F));
	CHECK(ScaledValue(1, 0, 0) == Catch::Approx(0.01F));     // never quite nothing
	CHECK(ScaledValue(-1, 0.5F, 0) == Catch::Approx(-0.5F)); // the sign (a mirror) is kept
	CHECK(ScaledValue(1, -2, 0) == Catch::Approx(-2.0F));    // dragging past zero mirrors
}
