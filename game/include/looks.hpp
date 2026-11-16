#pragma once

#include "assets.hpp"
#include "direction.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "graphics/animation.hpp"

// How the player looks, and the system that steps every animation. (The enemies' looks are in
// enemysystems.hpp.)
namespace game
{
	// Component: the way the player last moved, which it keeps facing when it stops.
	struct Facing
	{
		Direction direction = Direction::Down;
	};

	struct PlayerPose
	{
		engine::graphics::Clip clip{};
		bool flipX = false;
	};

	// The player's animation: walking the way it faces (left is right, mirrored), or its death
	// while dying. Pure.
	[[nodiscard]] PlayerPose PlayerPoseFor(const SpriteClips& clips, Direction facing, bool dying) noexcept;

	// LateUpdate (always): turns the player to face the way it moves and picks its animation.
	void PlayerLookSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);

	// LateUpdate (not while paused), after the look systems: steps every Animation by dt and
	// shows its frame in the entity's Sprite.
	void AnimationSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
