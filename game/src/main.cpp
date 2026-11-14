#include <algorithm>
#include <cstdint>

#include "application/application.hpp"
#include "controls.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/color.hpp"
#include "graphics/renderer.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"
#include "input/keyboard.hpp"

namespace
{
	using engine::ecs::Entity;
	using engine::graphics::Color;
	using engine::graphics::Position;
	using engine::graphics::Shape;
	using engine::graphics::Transform;

	constexpr int kLogicalWidth = 320;
	constexpr int kLogicalHeight = 180;
	constexpr float kSquareSize = 16.0f;
	constexpr float kPlayerSpeed = 90.0f; // logical pixels per second

	struct Velocity
	{
		float x = 0; // logical pixels per second
		float y = 0;
	};

	struct PlayerControlled
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

	void ClampToScreenSystem(engine::ecs::Registry& registry)
	{
		registry.Each<Transform, Shape>([](Entity, Transform& transform, const Shape& shape)
		{
			transform.position.x =
			    std::clamp(transform.position.x, 0.0f, static_cast<float>(kLogicalWidth) - shape.BoundsWidth());
			transform.position.y =
			    std::clamp(transform.position.y, 0.0f, static_cast<float>(kLogicalHeight) - shape.BoundsHeight());
		});
	}

	// Halves each channel, for drawing things while the game is paused.
	Color Dimmed(Color color)
	{
		return {static_cast<std::uint8_t>(color.r / 2), static_cast<std::uint8_t>(color.g / 2),
		        static_cast<std::uint8_t>(color.b / 2)};
	}

	// Test bed for input: a square steered with the arrow keys or WASD. P or Escape pauses.
	class MazeGame final : public engine::application::Game
	{
		engine::ecs::Registry registry;
		game::Controls controls;
		bool paused = false;

	public:
		MazeGame()
		{
			const Entity player = registry.Create();
			registry.Add<Transform>(player, Position{152.0f, 82.0f});
			registry.Add<Velocity>(player);
			registry.Add<PlayerControlled>(player);
			registry.Add<Shape>(player, Shape{.width = kSquareSize, .height = kSquareSize, .color = {255, 210, 0}});
		}

		void Update(double dt, const engine::input::Keyboard& keyboard) override
		{
			controls.Update(keyboard);
			if (controls.WasPressed(game::Action::Pause))
			{
				paused = !paused;
			}
			if (paused) return;

			PlayerInputSystem(registry, controls);
			MovementSystem(registry, static_cast<float>(dt));
			ClampToScreenSystem(registry);
		}

		void Render(engine::graphics::Renderer& renderer, double /*alpha*/) override
		{
			renderer.Clear(0, 0, 0);
			const bool dim = paused;
			const auto draw = [&renderer, dim](Entity, const Transform& transform, const Shape& shape)
			{
				const Color color = dim ? Dimmed(shape.color) : shape.color;
				renderer.FillRect(transform.position.x, transform.position.y, shape.BoundsWidth(), shape.BoundsHeight(),
				                  color.r, color.g, color.b);
			};
			registry.Each<Transform, Shape>(draw);
		}
	};
} // namespace

int main()
{
	engine::application::Application app({.title = "ecs-engine",
	                                      .windowWidth = 1280,
	                                      .windowHeight = 720,
	                                      .logicalWidth = kLogicalWidth,
	                                      .logicalHeight = kLogicalHeight});
	MazeGame game;
	return app.Run(game);
}
