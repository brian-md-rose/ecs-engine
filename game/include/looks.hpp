#pragma once

#include "assets.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "graphics/animation.hpp"
#include "grid/direction.hpp"

// How the player looks. (The enemies' looks are in enemysystems.hpp; every Animation is stepped
// by engine::graphics::AnimationSystem.)
namespace game
{
	// Component: the way the player last moved, which it keeps facing when it stops.
	struct Facing
	{
		engine::grid::Direction direction = engine::grid::Direction::Down;
	};

	struct PlayerPose
	{
		engine::graphics::Clip clip{};
		bool flipX = false;
	};

	// The player's animation: walking the way it faces (left is right, mirrored), or its death
	// while dying. Pure.
	[[nodiscard]] PlayerPose PlayerPoseFor(const SpriteClips& clips, engine::grid::Direction facing,
	                                       bool dying) noexcept;

	// LateUpdate (always): turns the player to face the way it moves and picks its animation.
	void PlayerLookSystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
