#include "mazegame.hpp"

#include "components.hpp"
#include "controls.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "gamestate.hpp"
#include "graphics/camera.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "systems.hpp"

namespace game
{
	MazeGame::MazeGame()
	{
		using engine::ecs::Phase;
		using engine::graphics::Position;
		using engine::graphics::Shape;
		using engine::graphics::Transform;

		world.resources.Emplace<Controls>();
		world.resources.Emplace<GameState>();
		world.resources.Emplace<FrameStats>();
		world.resources.Emplace<engine::graphics::Renderer2D>(
		    engine::graphics::Camera{.logicalWidth = kLogicalWidth, .logicalHeight = kLogicalHeight});

		engine::ecs::Registry& registry = world.registry;
		const engine::ecs::Entity player = registry.Create();
		registry.Add<Transform>(player,
		                        Position{(kLogicalWidth - kSquareSize) / 2.0f, (kLogicalHeight - kSquareSize) / 2.0f});
		registry.Add<Velocity>(player);
		registry.Add<PlayerControlled>(player);
		registry.Add<Shape>(
		    player, Shape{.width = kSquareSize, .height = kSquareSize, .color = {255, 210, 0}, .layer = kPlayerLayer});

		// The whole frame, in run order.
		scheduler.Add(Phase::Input, ControlsSystem);
		scheduler.Add(Phase::Input, GameStateSystem);
		scheduler.Add(Phase::Update, PlayerInputSystem, NotPaused);
		scheduler.Add(Phase::Update, MovementSystem, NotPaused);
		scheduler.Add(Phase::LateUpdate, ClampToScreenSystem, NotPaused);
		scheduler.Add(Phase::Render, RenderSystem);
	}
} // namespace game
