#include "application/application.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/renderer.hpp"

namespace
{
	constexpr int kLogicalWidth = 320;
	constexpr int kLogicalHeight = 180;
	constexpr float kSquareSize = 16.0f;

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

	void MovementSystem(engine::ecs::Registry& registry, float dt)
	{
		registry.Each<Position, Velocity>([dt](engine::ecs::Entity, Position& p, const Velocity& v) {
			p.x += v.x * dt;
			p.y += v.y * dt;
		});
	}

	// Keeps squares on screen by reversing their velocity at the edges.
	void BounceSystem(engine::ecs::Registry& registry)
	{
		constexpr float maxX = static_cast<float>(kLogicalWidth) - kSquareSize;
		constexpr float maxY = static_cast<float>(kLogicalHeight) - kSquareSize;

		registry.Each<Position, Velocity>([](engine::ecs::Entity, Position& p, Velocity& v) {
			if ((p.x < 0.0f && v.x < 0.0f) || (p.x > maxX && v.x > 0.0f)) v.x = -v.x;
			if ((p.y < 0.0f && v.y < 0.0f) || (p.y > maxY && v.y > 0.0f)) v.y = -v.y;
		});
	}

	// Test bed for the fixed-timestep loop: one square bouncing around the screen at a
	// constant speed, whatever the frame rate.
	class MazeGame final : public engine::application::Game
	{
		engine::ecs::Registry registry;

	public:
		MazeGame()
		{
			const engine::ecs::Entity player = registry.Create();
			registry.Add<Position>(player, 10.0f, 10.0f);
			registry.Add<Velocity>(player, 120.0f, 60.0f);
		}

		void Update(double dt) override
		{
			MovementSystem(registry, static_cast<float>(dt));
			BounceSystem(registry);
		}

		void Render(engine::graphics::Renderer& renderer, double /*alpha*/) override
		{
			renderer.Clear(0, 0, 0);
			registry.Each<Position>([&renderer](engine::ecs::Entity, const Position& p) {
				renderer.FillRect(p.x, p.y, kSquareSize, kSquareSize, 255, 210, 0);
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
