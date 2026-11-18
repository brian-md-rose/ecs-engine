#pragma once

#include <array>
#include <cstdint>

#include "ecs/entity.hpp"
#include "graphics/color.hpp"
#include "graphics/image.hpp"
#include "graphics/transform.hpp"

namespace engine::ui
{
	// Component: a panel, button or bar frame drawn from one small picture by stretching its middle and
	// edges and keeping its corners. `source` is the whole picture in the atlas; left, right, top and
	// bottom are the widths of the border that stays unstretched. NinePatchSystem draws it as nine
	// stretched sprites, children of this entity, so the entity needs a graphics::Transform, and
	// ScreenSpace or Clip on it apply to the pieces. The size is in the entity's units (like Sprite's).
	//
	// The pieces are children, so destroy a nine-patch with ecs::DestroyWithChildren. A plain Destroy leaves
	// the pieces behind, still drawn, with a parent that no longer exists.
	struct NinePatch
	{
		graphics::PixelRect source{};
		int left = 0;
		int right = 0;
		int top = 0;
		int bottom = 0;
		int width = 0;
		int height = 0;
		graphics::Color tint{};
		std::uint8_t layer = 0;
	};

	// Component, added by the system: the nine child entities of a NinePatch, row by row.
	struct NinePatchPieces
	{
		std::array<ecs::Entity, 9> pieces{};
		NinePatchPieces() { pieces.fill(ecs::Entity::Null); }
	};

	// One of the nine rectangles of a NinePatch laid out at its size.
	struct NinePatchPiece
	{
		graphics::PixelRect source{}; // the part of the picture it shows
		graphics::Position offset{};  // top-left, from the panel's top-left
		int width = 0;                // size drawn
		int height = 0;
		// False when there is nothing to draw: a border of 0, a size too small for it, or nothing
		// between the borders in the picture.
		[[nodiscard]] bool Visible() const noexcept
		{
			return width > 0 && height > 0 && source.width > 0 && source.height > 0;
		}
	};

	// The nine pieces, row by row from the top left, for a panel of patch.width x patch.height. Corners
	// keep their size, the edges stretch along one axis and the middle along both. If the panel is
	// smaller than its borders, the borders shrink in proportion so the pieces still fit exactly.
	//
	// Fills pieces rather than returning them: MSVC /Wall warns (C4686) about returning a std::array of
	// these by value.
	void NinePatchLayout(const NinePatch& patch, std::array<NinePatchPiece, 9>& pieces);
} // namespace engine::ui
