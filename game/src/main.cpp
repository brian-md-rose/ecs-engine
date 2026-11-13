#include <algorithm>
#include <cstdint>

#include "application/application.hpp"
#include "controls.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/renderer.hpp"
#include "input/keyboard.hpp"

namespace
{
	constexpr int kLogicalWidth = 320;
	constexpr int kLogicalHeight = 180;
	constexpr float kSquareSize = 16.0f;
	constexpr float kPlayerSpeed = 90.0f; // logical pixels per second

	struct Position
	{
		float x = 0;
		float y = 0;
	};

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

		registry.Each<PlayerControlled, Velocity>([heading](engine::ecs::Entity, const PlayerControlled&, Velocity& velocity)
		{
			velocity = heading;
		});
	}

	void MovementSystem(engine::ecs::Registry& registry, float dt)
	{
		registry.Each<Position, Velocity>([dt](engine::ecs::Entity, Position& p, const Velocity& v) {
			p.x += v.x * dt;
			p.y += v.y * dt;
		});
	}

	// Stops entities at the screen edges.
	constexpr float kMaxX = static_cast<float>(kLogicalWidth) - kSquareSize;
	constexpr float kMaxY = static_cast<float>(kLogicalHeight) - kSquareSize;

	void ClampToScreenSystem(engine::ecs::Registry& registry)
	{
		registry.Each<Position>([](engine::ecs::Entity, Position& p)
		{
			p.x = std::clamp(p.x, 0.0f, kMaxX);
			p.y = std::clamp(p.y, 0.0f, kMaxY);
		});
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
			const engine::ecs::Entity player = registry.Create();
			registry.Add<Position>(player, 152.0f, 82.0f);
			registry.Add<Velocity>(player);
			registry.Add<PlayerControlled>(player);
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
			// Yellow, dimmed while paused.
			const std::uint8_t red = paused ? 128 : 255;
			const std::uint8_t green = paused ? 105 : 210;

			registry.Each<Position>([&renderer, red, green](engine::ecs::Entity, const Position& p)
			{
				renderer.FillRect(p.x, p.y, kSquareSize, kSquareSize, red, green, 0);
			});
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
