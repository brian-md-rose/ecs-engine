#include <cstdint>
#include <cstdio>

#include "ecs/component.hpp"
#include "ecs/componentpool.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/renderer.hpp"
#include "graphics/window.hpp"

struct Position
{
	float x = 0;
	float y = 0;
};

struct Velocity
{
	float x = 0;
	float y = 0;
};

void MovementSystem(engine::ecs::Registry& registry, float dt)
{
	registry.Each<Position, Velocity>([dt](engine::ecs::Entity, Position& p, const Velocity& v) {
		p.x += v.x * dt;
		p.y += v.y * dt;
	});
}

int main()
{
	engine::ecs::Registry registry;
	engine::ecs::Entity player = registry.Create();

	registry.Add<Position>(player, 10.0f, 10.0f);
	registry.Add<Velocity>(player, 0.0f, -9.8f);

	const Position& position = registry.GetComponent<Position>(player);

	std::printf("ecs-engine: entity %u (generation %u) at (%.1f, %.1f)\n", static_cast<unsigned>(player.id),
	            static_cast<unsigned>(player.generation), static_cast<double>(position.x),
	            static_cast<double>(position.y));

	MovementSystem(registry, 1.0f);

	engine::graphics::Window window("ecs-engine", 1280, 720);
	engine::graphics::Renderer renderer(window, 320, 180);

	while (window.IsValid() && renderer.IsValid() && window.PollEvents())
	{
		renderer.Clear(0, 0, 0);
		renderer.Present();
	}

	return 0;
}
