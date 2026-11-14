#include "mazegame.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>

#include "controls.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/camera.hpp"
#include "graphics/color.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::Entity;
		using engine::ecs::World;
		using engine::graphics::Color;
		using engine::graphics::Position;
		using engine::graphics::Shape;
		using engine::graphics::ShapeKind;
		using engine::graphics::Transform;

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

		// Game-wide flags, kept as a resource.
		struct GameState
		{
			bool paused = false;
			bool stressTest = false;
		};

		// Steers player-controlled entities toward the last direction pressed. They keep moving
		// after the key is released, the way the maze game's player will.
		void PlayerInputSystem(World& world)
		{
			const auto& controls = world.resources.Get<Controls>();

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

			world.registry.Each<PlayerControlled, Velocity>(
			    [heading](Entity, const PlayerControlled&, Velocity& velocity)
			{
				velocity = heading;
			});
		}

		void MovementSystem(World& world, float dt)
		{
			world.registry.Each<Transform, Velocity>([dt](Entity, Transform& transform, const Velocity& v)
			{
				transform.position.x += v.x * dt;
				transform.position.y += v.y * dt;
			});
		}

		// Stops moving entities at the screen edges.
		void ClampToScreenSystem(World& world)
		{
			world.registry.Each<Transform, Shape, Velocity>(
			    [](Entity, Transform& transform, const Shape& shape, const Velocity&)
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
		void SpawnStressShapes(World& world)
		{
			engine::ecs::Registry& registry = world.registry;
			std::mt19937 random(12345u);
			std::uniform_real_distribution<float> x(0.0f, static_cast<float>(kLogicalWidth) - 8.0f);
			std::uniform_real_distribution<float> y(0.0f, static_cast<float>(kLogicalHeight) - 8.0f);
			std::uniform_real_distribution<float> size(2.0f, 8.0f);
			std::uniform_int_distribution<int> channel(40, 255);
			std::uniform_int_distribution<int> layer(0, kPlayerLayer - 1);
			std::bernoulli_distribution circle(0.5);

			for (int i = 0; i < kStressShapeCount; ++i)
			{
				const Color color{static_cast<std::uint8_t>(channel(random)),
				                  static_cast<std::uint8_t>(channel(random)),
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

		// Destroying inside Each would invalidate the loop, so the destroys are queued and run when
		// the caller applies world.commands.
		void DestroyStressShapes(World& world)
		{
			world.registry.Each<StressShape>([&world](Entity entity, const StressShape&)
			{
				world.commands.Destroy(entity);
			});
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

		// Steps the controls from the keyboard. Runs first, so every later system sees this
		// update's actions.
		void ControlsSystem(World& world, const engine::input::Keyboard& keyboard)
		{
			world.resources.Get<Controls>().Update(keyboard);
		}

		// Pause and the stress-test toggle. Runs even while paused, so the game can be unpaused.
		void GameStateSystem(World& world)
		{
			const auto& controls = world.resources.Get<Controls>();
			auto& state = world.resources.Get<GameState>();

			if (controls.WasPressed(Action::Pause))
			{
				state.paused = !state.paused;
			}
			if (controls.WasPressed(Action::ToggleStressTest))
			{
				state.stressTest = !state.stressTest;
				if (state.stressTest)
				{
					SpawnStressShapes(world);
					world.resources.Get<FrameStats>().Reset();
				}
				else
				{
					DestroyStressShapes(world);
				}
			}
		}

		// Draws every shape, dimmed while paused, and reports frame times during the stress test.
		void RenderSystem(World& world, engine::graphics::Renderer& renderer)
		{
			const auto& state = world.resources.Get<GameState>();
			auto& renderer2D = world.resources.Get<engine::graphics::Renderer2D>();

			renderer.Clear(0, 0, 0);
			renderer2D.SetTint(state.paused ? kPausedTint : Color{});

			const auto drawStart = std::chrono::steady_clock::now();
			renderer2D.Draw(renderer, world.registry);
			if (state.stressTest)
			{
				world.resources.Get<FrameStats>().AddFrame(std::chrono::steady_clock::now() - drawStart,
				                                           renderer2D.ShapeCount());
			}
		}
	} // namespace

	MazeGame::MazeGame()
	{
		world.resources.Emplace<Controls>();
		world.resources.Emplace<GameState>();
		world.resources.Emplace<FrameStats>();
		world.resources.Emplace<engine::graphics::Renderer2D>(
		    engine::graphics::Camera{.logicalWidth = kLogicalWidth, .logicalHeight = kLogicalHeight});

		engine::ecs::Registry& registry = world.registry;
		const Entity player = registry.Create();
		registry.Add<Transform>(player,
		                        Position{(kLogicalWidth - kSquareSize) / 2.0f, (kLogicalHeight - kSquareSize) / 2.0f});
		registry.Add<Velocity>(player);
		registry.Add<PlayerControlled>(player);
		registry.Add<Shape>(
		    player, Shape{.width = kSquareSize, .height = kSquareSize, .color = {255, 210, 0}, .layer = kPlayerLayer});
	}

	void MazeGame::Update(double dt, const engine::input::Keyboard& keyboard)
	{
		ControlsSystem(world, keyboard);
		GameStateSystem(world);
		world.commands.Apply(world.registry);
		if (world.resources.Get<GameState>().paused) return;

		PlayerInputSystem(world);
		MovementSystem(world, static_cast<float>(dt));
		ClampToScreenSystem(world);
	}

	void MazeGame::Render(engine::graphics::Renderer& renderer, double /*alpha*/)
	{
		RenderSystem(world, renderer);
	}
} // namespace game
