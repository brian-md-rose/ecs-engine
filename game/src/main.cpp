#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include "application/application.hpp"
#include "controls.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/camera.hpp"
#include "graphics/color.hpp"
#include "graphics/renderer.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "input/keyboard.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::graphics::Color;
	using engine::graphics::Position;
	using engine::graphics::Shape;
	using engine::graphics::ShapeKind;
	using engine::graphics::Transform;

	// Portrait, the shape of a classic maze screen, scaled 3x into the window.
	constexpr int kLogicalWidth = 224;
	constexpr int kLogicalHeight = 288;
	constexpr int kWindowScale = 3;
	constexpr float kSquareSize = 16.0f;
	constexpr float kPlayerSpeed = 90.0f; // logical pixels per second

	constexpr std::uint8_t kPlayerLayer = 3; // above every stress-test shape
	constexpr int kStressShapeCount = 1000;
	constexpr Color kPausedTint{128, 128, 128};

	struct Velocity
	{
		float x = 0; // logical pixels per second
		float y = 0;
	};

	struct PlayerControlled
	{
	};

	struct StressShape
	{
	};

	// Steers player-controlled entities toward the last direction pressed. They keep moving
	// after the key is released, the way the maze game's player will.
	void PlayerInputSystem(engine::ecs::Registry& registry, const game::Controls& controls)
	{
		using game::Direction;

		Velocity heading{};
		switch (controls.BufferedDirection())
		{
		case Direction::Up:
			heading = {0.0f, -kPlayerSpeed};
			break;
		case Direction::Down:
			heading = {0.0f, kPlayerSpeed};
			break;
		case Direction::Left:
			heading = {-kPlayerSpeed, 0.0f};
			break;
		case Direction::Right:
			heading = {kPlayerSpeed, 0.0f};
			break;
		case Direction::None:
			break;
		}

		registry.Each<PlayerControlled, Velocity>([heading](Entity, const PlayerControlled&, Velocity& velocity)
		{
			velocity = heading;
		});
	}

	void MovementSystem(engine::ecs::Registry& registry, float dt)
	{
		registry.Each<Transform, Velocity>([dt](Entity, Transform& transform, const Velocity& v)
		{
			transform.position.x += v.x * dt;
			transform.position.y += v.y * dt;
		});
	}

	// Stops moving entities at the screen edges.
	void ClampToScreenSystem(engine::ecs::Registry& registry)
	{
		registry.Each<Transform, Shape, Velocity>([](Entity, Transform& transform, const Shape& shape, const Velocity&)
		{
			transform.position.x =
			    std::clamp(transform.position.x, 0.0f, static_cast<float>(kLogicalWidth) - shape.BoundsWidth());
			transform.position.y =
			    std::clamp(transform.position.y, 0.0f, static_cast<float>(kLogicalHeight) - shape.BoundsHeight());
		});
	}

	// Debug: fills the screen with random rectangles and circles on layers below the player,
	// to check that hundreds of shapes draw at a steady frame rate. A fixed seed keeps runs
	// comparable.
	void SpawnStressShapes(engine::ecs::Registry& registry)
	{
		std::mt19937 random(12345u);
		std::uniform_real_distribution<float> x(0.0f, static_cast<float>(kLogicalWidth) - 8.0f);
		std::uniform_real_distribution<float> y(0.0f, static_cast<float>(kLogicalHeight) - 8.0f);
		std::uniform_real_distribution<float> size(2.0f, 8.0f);
		std::uniform_int_distribution<int> channel(40, 255);
		std::uniform_int_distribution<int> layer(0, kPlayerLayer - 1);
		std::bernoulli_distribution circle(0.5);

		for (int i = 0; i < kStressShapeCount; ++i)
		{
			const Color color{static_cast<std::uint8_t>(channel(random)), static_cast<std::uint8_t>(channel(random)),
			                  static_cast<std::uint8_t>(channel(random))};
			const auto shapeLayer = static_cast<std::uint8_t>(layer(random));
			const float extent = size(random);
			const Shape shape =
			    circle(random)
			        ? Shape{.kind = ShapeKind::Circle, .radius = extent / 2.0f, .color = color, .layer = shapeLayer}
			        : Shape{.width = extent, .height = extent, .color = color, .layer = shapeLayer};

			const Entity entity = registry.Create();
			registry.Add<Transform>(entity, Position{x(random), y(random)});
			registry.Add<Shape>(entity, shape);
			registry.Add<StressShape>(entity);
		}
	}

	void DestroyStressShapes(engine::ecs::Registry& registry)
	{
		// Destroying inside Each would invalidate the loop, so collect first.
		std::vector<Entity> doomed;
		registry.Each<StressShape>([&doomed](Entity entity, const StressShape&)
		{
			doomed.push_back(entity);
		});
		for (const Entity entity : doomed)
		{
			registry.Destroy(entity);
		}
	}

	// Prints frames per second and the average time spent in Renderer2D::Draw, once a second.
	class FrameStats
	{
		using Clock = std::chrono::steady_clock;

		Clock::time_point windowStart = Clock::now();
		Clock::duration drawTime{};
		int frames = 0;

	public:
		void Reset()
		{
			windowStart = Clock::now();
			drawTime = {};
			frames = 0;
		}

		void AddFrame(Clock::duration draw, std::size_t shapes)
		{
			drawTime += draw;
			++frames;

			const Clock::time_point now = Clock::now();
			const std::chrono::duration<double> elapsed = now - windowStart;
			if (elapsed.count() < 1.0)
			{
				return;
			}

			const std::chrono::duration<double, std::milli> averageDraw = drawTime / frames;
			std::printf("stress test: %zu shapes, %.1f fps, draw %.3f ms/frame\n", shapes,
			            static_cast<double>(frames) / elapsed.count(), averageDraw.count());
			std::fflush(stdout);
			Reset();
		}
	};

	// Test bed for rendering and input: a square steered with the arrow keys or WASD.
	// P or Escape pauses; F1 toggles the rendering stress test.
	class MazeGame final : public engine::application::Game
	{
		engine::ecs::Registry registry;
		engine::graphics::Renderer2D renderer2D{{.logicalWidth = kLogicalWidth, .logicalHeight = kLogicalHeight}};
		game::Controls controls;
		FrameStats stats;
		bool paused = false;
		bool stressTest = false;

	public:
		MazeGame()
		{
			const Entity player = registry.Create();
			registry.Add<Transform>(
			    player, Position{(kLogicalWidth - kSquareSize) / 2.0f, (kLogicalHeight - kSquareSize) / 2.0f});
			registry.Add<Velocity>(player);
			registry.Add<PlayerControlled>(player);
			registry.Add<Shape>(
			    player,
			    Shape{.width = kSquareSize, .height = kSquareSize, .color = {255, 210, 0}, .layer = kPlayerLayer});
		}

		void Update(double dt, const engine::input::Keyboard& keyboard) override
		{
			controls.Update(keyboard);
			if (controls.WasPressed(game::Action::Pause))
			{
				paused = !paused;
			}
			if (controls.WasPressed(game::Action::ToggleStressTest))
			{
				stressTest = !stressTest;
				if (stressTest)
				{
					SpawnStressShapes(registry);
					stats.Reset();
				}
				else
				{
					DestroyStressShapes(registry);
				}
			}
			if (paused) return;

			PlayerInputSystem(registry, controls);
			MovementSystem(registry, static_cast<float>(dt));
			ClampToScreenSystem(registry);
		}

		void Render(engine::graphics::Renderer& renderer, double /*alpha*/) override
		{
			renderer.Clear(0, 0, 0);
			renderer2D.SetTint(paused ? kPausedTint : Color{});

			const auto drawStart = std::chrono::steady_clock::now();
			renderer2D.Draw(renderer, registry);
			if (stressTest)
			{
				stats.AddFrame(std::chrono::steady_clock::now() - drawStart, renderer2D.ShapeCount());
			}
		}
	};
} // namespace

int main()
{
	engine::application::Application app({.title = "ecs-engine",
	                                      .windowWidth = kLogicalWidth * kWindowScale,
	                                      .windowHeight = kLogicalHeight * kWindowScale,
	                                      .logicalWidth = kLogicalWidth,
	                                      .logicalHeight = kLogicalHeight});
	MazeGame game;
	return app.Run(game);
}
