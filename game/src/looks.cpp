#include "looks.hpp"

#include "components.hpp"
#include "ecs/entity.hpp"
#include "ecs/states.hpp"
#include "gameflow.hpp"
#include "graphics/sprite.hpp"

namespace game
{
	using engine::grid::Direction;

	namespace
	{
		using engine::ecs::Entity;
		using engine::ecs::FrameContext;
		using engine::ecs::World;
		using engine::graphics::Animation;
		using engine::graphics::Sprite;
	} // namespace

	PlayerPose PlayerPoseFor(const SpriteClips& clips, Direction facing, bool dying) noexcept
	{
		if (dying) return {.clip = clips.playerDeath};
		switch (facing)
		{
		case Direction::Up:
			return {.clip = clips.playerUp};
		case Direction::Left:
			return {.clip = clips.playerRight, .flipX = true};
		case Direction::Right:
			return {.clip = clips.playerRight};
		case Direction::Down:
		case Direction::None:
			break;
		}
		return {.clip = clips.playerDown};
	}

	void PlayerLookSystem(World& world, const FrameContext&)
	{
		const SpriteClips& clips = world.resources.Get<SpriteClips>();
		const bool dying = engine::ecs::InState<Round::Dying>(world);
		world.registry.Each<PlayerControlled, GridMover, Facing, Animation, Sprite>(
		    [&](Entity, const PlayerControlled&, const GridMover& mover, Facing& facing, Animation& animation,
		        Sprite& sprite)
		{
			if (mover.direction != Direction::None) facing.direction = mover.direction;
			const PlayerPose pose = PlayerPoseFor(clips, facing.direction, dying);
			animation = Play(animation, pose.clip);
			sprite.flipX = pose.flipX;
		});
	}
} // namespace game
