#include "graphics/shapebatch.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ecs/entity.hpp"
#include "ecs/registry.hpp"

namespace engine::graphics
{
	namespace
	{
		bool Overlaps(Position topLeft, float width, float height, const Camera& camera)
		{
			return topLeft.x < static_cast<float>(camera.logicalWidth) && topLeft.x + width > 0.0f &&
			       topLeft.y < static_cast<float>(camera.logicalHeight) && topLeft.y + height > 0.0f;
		}
	} // namespace

	void ShapeBatch::Build(ecs::Registry& registry, const Camera& camera)
	{
		entries.clear();
		vertices.clear();
		indices.clear();

		const auto collect = [this, &camera](ecs::Entity, const Transform& transform, const Shape& shape)
		{
			const Position screen{transform.position.x - camera.position.x, transform.position.y - camera.position.y};
			if (Overlaps(screen, shape.BoundsWidth(), shape.BoundsHeight(), camera))
			{
				entries.push_back({screen, shape});
			}
		};
		registry.Each<Transform, Shape>(collect);

		const auto byLayer = [](const Entry& a, const Entry& b)
		{
			return a.shape.layer < b.shape.layer;
		};
		std::ranges::stable_sort(entries, byLayer);

		for (const Entry& entry : entries)
		{
			if (entry.shape.kind == ShapeKind::Circle)
			{
				AddCircle(entry);
			}
			else
			{
				AddRectangle(entry);
			}
		}
	}

	void ShapeBatch::AddRectangle(const Entry& entry)
	{
		const int first = static_cast<int>(vertices.size());
		const float left = entry.screen.x;
		const float top = entry.screen.y;
		const float right = left + entry.shape.width;
		const float bottom = top + entry.shape.height;
		const Color color = entry.shape.color;

		vertices.push_back({{left, top}, color});
		vertices.push_back({{right, top}, color});
		vertices.push_back({{right, bottom}, color});
		vertices.push_back({{left, bottom}, color});

		for (const int corner : {0, 1, 2, 0, 2, 3})
		{
			indices.push_back(first + corner);
		}
	}

	void ShapeBatch::AddCircle(const Entry& entry)
	{
		// A fan around the centre: one centre vertex, kCircleSegments rim vertices, and one
		// triangle per segment.
		const int centre = static_cast<int>(vertices.size());
		const float radius = entry.shape.radius;
		const float cx = entry.screen.x + radius;
		const float cy = entry.screen.y + radius;
		const Color color = entry.shape.color;

		vertices.push_back({{cx, cy}, color});
		for (int i = 0; i < kCircleSegments; ++i)
		{
			const float angle =
			    2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(kCircleSegments);
			vertices.push_back({{cx + radius * std::cos(angle), cy + radius * std::sin(angle)}, color});
		}

		for (int i = 0; i < kCircleSegments; ++i)
		{
			indices.push_back(centre);
			indices.push_back(centre + 1 + i);
			indices.push_back(centre + 1 + (i + 1) % kCircleSegments);
		}
	}
} // namespace engine::graphics
