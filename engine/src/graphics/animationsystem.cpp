#include "graphics/animationsystem.hpp"

#include "ecs/entity.hpp"
#include "graphics/animation.hpp"
#include "graphics/sprite.hpp"

namespace engine::graphics
{
	void AnimationSystem(ecs::World& world, const ecs::FrameContext& context)
	{
		const auto dt = static_cast<float>(context.dt);
		world.registry.Each<Animation, Sprite>([dt](ecs::Entity, Animation& animation, Sprite& sprite)
		{
			animation = Advance(animation, dt);
			sprite.source = AnimationSource(animation);
		});
	}
} // namespace engine::graphics
