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
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
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
