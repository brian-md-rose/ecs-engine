#include "ui/ui.hpp"

#include <algorithm>

#include "ecs/registry.hpp"
#include "graphics/ancestry.hpp"

namespace engine::ui
{
	float ClampScroll(float offset, float contentHeight, float viewHeight) noexcept
	{
		const float most = std::max(contentHeight - viewHeight, 0.0f);
		return std::clamp(offset, 0.0f, most);
	}

	PointerPoint PointerInSpaceOf(ecs::Registry& registry, ecs::Entity entity, const input::Mouse& mouse,
	                              const graphics::Camera& camera)
	{
		const graphics::Ancestry ancestry = graphics::ResolveAncestry(registry, entity);
		PointerPoint point;
		point.position = ancestry.screenSpace ? mouse.position : camera.ScreenToWorld(mouse.position);
		if (ancestry.clipped)
		{
			// The clip is in screen pixels whatever space the entity is in.
			const graphics::PixelRect& clip = ancestry.clip;
			point.inClip = mouse.position.x >= static_cast<float>(clip.x) &&
			               mouse.position.x < static_cast<float>(clip.x + clip.width) &&
			               mouse.position.y >= static_cast<float>(clip.y) &&
			               mouse.position.y < static_cast<float>(clip.y + clip.height);
		}
		return point;
	}

	bool PointerHits(ecs::Registry& registry, ecs::Entity entity, const graphics::Transform& transform,
	                 const Hitbox& hitbox, const input::Mouse& mouse, const graphics::Camera& camera)
	{
		if (!mouse.inside) return false;
		const PointerPoint point = PointerInSpaceOf(registry, entity, mouse, camera);
		if (!point.inClip) return false;
		const graphics::Position& at = transform.position;
		return point.position.x >= at.x && point.position.x < at.x + hitbox.width && point.position.y >= at.y &&
		       point.position.y < at.y + hitbox.height;
	}
} // namespace engine::ui
