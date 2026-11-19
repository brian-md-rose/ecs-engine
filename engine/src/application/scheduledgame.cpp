#include "application/scheduledgame.hpp"

#include "graphics/interpolation.hpp"
#include "graphics/transform.hpp"

namespace engine::application
{
	void ScheduledGame::Update(double dt, const input::Input& input, audio::AudioDevice* audio)
	{
		const ecs::FrameContext context{.dt = dt, .input = &input, .audio = audio};
		// First, so a reset made by a state change is not interpolated from where things were.
		scheduler.RunTransitions(world, context);
		graphics::RecordPreviousPositions(world.registry);
		scheduler.Run(ecs::Phase::Input, world, context);
		scheduler.Run(ecs::Phase::Update, world, context);
		scheduler.Run(ecs::Phase::LateUpdate, world, context);
		// Children follow their parents once everything has moved.
		graphics::UpdateWorldTransforms(world.registry);
	}

	void ScheduledGame::Render(graphics::Renderer& renderer, double alpha)
	{
		const ecs::FrameContext context{.alpha = alpha, .renderer = &renderer};
		// Again, so anything moved outside an update (and a frame drawn before the first) is right.
		graphics::UpdateWorldTransforms(world.registry);
		scheduler.Run(ecs::Phase::Render, world, context);
	}

	DisplayLink ScheduledGame::Display()
	{
		return {.request = world.resources.TryGet<graphics::DisplayRequest>(),
		        .info = world.resources.TryGet<graphics::DisplayInfo>()};
	}
} // namespace engine::application
