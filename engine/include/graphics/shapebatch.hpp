#pragma once
#include <cstddef>
#include <span>
#include <vector>

#include "graphics/camera.hpp"
#include "graphics/color.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"

namespace engine::ecs
{
	class Registry;
}

namespace engine::graphics
{
	// A corner of a triangle, in screen (logical-pixel) coordinates.
	struct Vertex
	{
		Position position;
		Color color;
	};

	// Turns every Transform + Shape entity into one indexed triangle list, so all shapes can
	// be drawn in a single call. Pure: no SDL, so the tests can check it directly.
	//
	// Shapes are drawn in ascending layer order. Within a layer the order is the registry's
	// iteration order, kept stable by the sort. Shapes entirely outside the camera are skipped.
	class ShapeBatch
	{
		struct Entry
		{
			Position screen; // top-left of the bounding box, camera applied
			Shape shape;
		};

		std::vector<Entry> entries;
		std::vector<Vertex> vertices;
		std::vector<int> indices; // int because SDL_RenderGeometry takes int indices

		void AddRectangle(const Entry& entry);
		void AddCircle(const Entry& entry);

	public:
		// Rim points per circle: enough to look round at 3-4x scale without many vertices.
		static constexpr int kCircleSegments = 24;

		// Rebuilds the batch from the registry. Only reads components; non-const because
		// Registry::Each is. The buffers are reused, so this doesn't allocate once warm.
		void Build(ecs::Registry& registry, const Camera& camera);

		[[nodiscard]] std::span<const Vertex> Vertices() const noexcept { return vertices; }
		[[nodiscard]] std::span<const int> Indices() const noexcept { return indices; }

		// Shapes that made it into the batch (after culling).
		[[nodiscard]] std::size_t ShapeCount() const noexcept { return entries.size(); }
	};
} // namespace engine::graphics
