#include "application/scheduledgame.hpp"

namespace engine::application
{
	void ScheduledGame::Update(double dt, const input::Keyboard& keyboard, audio::AudioDevice* audio)
	{
		const ecs::FrameContext context{.dt = dt, .keyboard = &keyboard, .audio = audio};
		scheduler.Run(ecs::Phase::Input, world, context);
		scheduler.Run(ecs::Phase::Update, world, context);
		scheduler.Run(ecs::Phase::LateUpdate, world, context);
	}

	void ScheduledGame::Render(graphics::Renderer& renderer, double alpha)
	{
		const ecs::FrameContext context{.alpha = alpha, .renderer = &renderer};
		scheduler.Run(ecs::Phase::Render, world, context);
	}
} // namespace engine::application
