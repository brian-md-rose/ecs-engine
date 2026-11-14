#include "systems.hpp"

#include <chrono>
#include <cstdint>
#include <random>

#include "components.hpp"
#include "controls.hpp"
#include "core/assert.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "gamestate.hpp"
#include "graphics/color.hpp"
#include "graphics/renderer.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "gridmovement.hpp"
#include "mazegame.hpp"
#include "tilemap.hpp"

namespace game
{
	namespace
	{
		using engine::ecs::Entity;
		using engine::ecs::FrameContext;
		using engine::ecs::World;
		using engine::graphics::Color;
		using engine::graphics::Position;
		using engine::graphics::Shape;
		using engine::graphics::ShapeKind;
		using engine::graphics::Transform;

		// Debug: fills the screen with random rectangles and circles on layers below the
		// player, to check that hundreds of shapes draw at a steady frame rate. A fixed seed
		// keeps runs comparable.
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

				// Queued, like every structural change a system makes; the scheduler applies
				// them at the end of the phase.
				const Entity entity = registry.Create();
				world.commands.Add<Transform>(entity, Position{x(random), y(random)});
				world.commands.Add<Shape>(entity, shape);
				world.commands.Add<StressShape>(entity);
			}
		}

		void DestroyStressShapes(World& world)
		{
			world.registry.Each<StressShape>([&world](Entity entity, const StressShape&)
			{
				world.commands.Destroy(entity);
			});
		}
	} // namespace

	bool NotPaused(const World& world)
	{
		return !world.resources.Get<GameState>().paused;
	}

	void ControlsSystem(World& world, const FrameContext& context)
	{
		ENGINE_VERIFY(context.keyboard != nullptr, "ControlsSystem must run in a fixed-rate phase");
		world.resources.Get<Controls>().Update(*context.keyboard);
	}

	void GameStateSystem(World& world, const FrameContext&)
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

	void PlayerInputSystem(World& world, const FrameContext&)
	{
		const Direction asked = world.resources.Get<Controls>().BufferedDirection();
		world.registry.Each<PlayerControlled, GridMover>([asked](Entity, const PlayerControlled&, GridMover& mover)
		{
			mover.buffered = asked;
		});
	}

	void GridMovementSystem(World& world, const FrameContext& context)
	{
		const auto dt = static_cast<float>(context.dt);
		const auto& maze = world.resources.Get<TileMap>();
		const Position origin = MazeOrigin(maze);
		world.registry.Each<GridMover, Transform>([&](Entity, GridMover& mover, Transform& transform)
		{
			mover = StepGridMover(mover, maze, dt);
			transform.position = MoverTopLeft(origin, mover);
		});
	}

	void RenderSystem(World& world, const FrameContext& context)
	{
		ENGINE_VERIFY(context.renderer != nullptr, "RenderSystem must run in the Render phase");
		engine::graphics::Renderer& renderer = *context.renderer;
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
} // namespace game
