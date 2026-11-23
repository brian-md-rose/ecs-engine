#include "graphics/drawbatch.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/registry.hpp"
#include "graphics/ancestry.hpp"
#include "graphics/clip.hpp"
#include "graphics/interpolation.hpp"
#include "graphics/screenspace.hpp"
#include "graphics/sprite.hpp"

namespace engine::graphics
{
	namespace
	{
		[[nodiscard]] bool Overlaps(Position topLeft, float width, float height, const Camera& camera)
		{
			return topLeft.x < static_cast<float>(camera.logicalWidth) && topLeft.x + width > 0.0f &&
			       topLeft.y < static_cast<float>(camera.logicalHeight) && topLeft.y + height > 0.0f;
		}

		// Whether a width x height box at topLeft overlaps rect.
		[[nodiscard]] bool OverlapsRect(Position topLeft, float width, float height, const PixelRect& rect)
		{
			return topLeft.x < static_cast<float>(rect.x + rect.width) &&
			       topLeft.x + width > static_cast<float>(rect.x) &&
			       topLeft.y < static_cast<float>(rect.y + rect.height) &&
			       topLeft.y + height > static_cast<float>(rect.y);
		}

		[[nodiscard]] Position Snap(Position position)
		{
			return {std::floor(position.x + 0.5f), std::floor(position.y + 0.5f)};
		}

		// The corner at (dx, dy) from origin of a box turned by an angle (cosine c, sine s) and stretched by k.
		[[nodiscard]] Position Corner(Position origin, float c, float s, Scale k, float dx, float dy)
		{
			const float sx = dx * k.x;
			const float sy = dy * k.y;
			return {origin.x + sx * c - sy * s, origin.y + sx * s + sy * c};
		}

		[[nodiscard]] float Radians(float degrees)
		{
			return degrees * (std::numbers::pi_v<float> / 180.0f);
		}

		// The box (top-left and size) that surrounds a width x height box at origin after it is turned
		// and stretched. Untouched when there is neither.
		struct Extent
		{
			Position topLeft;
			float width;
			float height;
		};
		[[nodiscard]] Extent Surround(Position origin, float width, float height, float rotation, Scale k)
		{
			if (rotation == 0.0f && k.x == 1.0f && k.y == 1.0f) return {origin, width, height};
			const float c = std::cos(Radians(rotation));
			const float s = std::sin(Radians(rotation));
			const Position a = Corner(origin, c, s, k, 0.0f, 0.0f);
			const Position b = Corner(origin, c, s, k, width, 0.0f);
			const Position d = Corner(origin, c, s, k, width, height);
			const Position e = Corner(origin, c, s, k, 0.0f, height);
			const float left = std::min({a.x, b.x, d.x, e.x});
			const float right = std::max({a.x, b.x, d.x, e.x});
			const float top = std::min({a.y, b.y, d.y, e.y});
			const float bottom = std::max({a.y, b.y, d.y, e.y});
			return {{left, top}, right - left, bottom - top};
		}

		[[nodiscard]] std::uint8_t Multiply(std::uint8_t a, std::uint8_t b)
		{
			return static_cast<std::uint8_t>((static_cast<unsigned>(a) * static_cast<unsigned>(b) + 127u) / 255u);
		}

		[[nodiscard]] Color Multiply(Color a, Color b)
		{
			return {Multiply(a.r, b.r), Multiply(a.g, b.g), Multiply(a.b, b.b)};
		}
	} // namespace

	void DrawBatch::Build(ecs::Registry& registry, const Camera& camera)
	{
		Build(registry, BatchSettings{.camera = camera});
	}

	void DrawBatch::Build(ecs::Registry& registry, const BatchSettings& settings)
	{
		entries.clear();
		runs.clear();
		vertices.clear();
		indices.clear();
		const Camera& camera = settings.camera;
		const bool hasAtlas = settings.atlasWidth > 0 && settings.atlasHeight > 0;

		// Where an entity is drawn: blended between updates if it has an Interpolation.
		const auto worldPosition = [&registry, alpha = settings.alpha](ecs::Entity entity, const Transform& transform)
		{
			const Interpolation* interpolation = registry.TryGetComponent<Interpolation>(entity);
			return interpolation == nullptr ? transform.position
			                                : InterpolatedPosition(transform, *interpolation, alpha);
		};

		// Where entity's top-left lands on the screen, the zoom it is drawn at, and the clip it is drawn
		// inside. The camera places and magnifies world-space entities; screen-space ones stay where
		// their Transform says, at 1. The clip is its own and its ancestors' ClipRect rects, intersected.
		struct Placement
		{
			Position screen;
			int zoom;
			bool clipped;
			PixelRect clip;
		};
		const auto place = [&registry, &camera, &worldPosition](ecs::Entity entity, const Transform& transform)
		{
			const Position world = worldPosition(entity, transform);
			const Ancestry ancestry = ResolveAncestry(registry, entity);
			if (ancestry.screenSpace)
			{
				return Placement{.screen = world, .zoom = 1, .clipped = ancestry.clipped, .clip = ancestry.clip};
			}
			return Placement{.screen = camera.WorldToScreen(world),
			                 .zoom = camera.EffectiveZoom(),
			                 .clipped = ancestry.clipped,
			                 .clip = ancestry.clip};
		};
		// Whether a width x height box at topLeft shows: inside the view and inside its clip.
		const auto visible = [&camera](const Placement& placement, Position topLeft, float width, float height)
		{
			return Overlaps(topLeft, width, height, camera) &&
			       (!placement.clipped || OverlapsRect(topLeft, width, height, placement.clip));
		};

		const auto collectShape =
		    [this, &place, &visible](ecs::Entity entity, const Transform& transform, const Shape& shape)
		{
			const Placement placement = place(entity, transform);
			Shape drawn = shape;
			const auto factor = static_cast<float>(placement.zoom);
			drawn.width *= factor;
			drawn.height *= factor;
			drawn.radius *= factor;
			const Extent extent = Surround(placement.screen, drawn.BoundsWidth(), drawn.BoundsHeight(),
			                               transform.rotation, transform.scale);
			if (!visible(placement, extent.topLeft, extent.width, extent.height)) return;
			entries.push_back({.screen = placement.screen,
			                   .layer = shape.layer,
			                   .color = shape.color,
			                   .clipped = placement.clipped,
			                   .clip = placement.clip,
			                   .shape = drawn,
			                   .rotation = transform.rotation,
			                   .scale = transform.scale});
		};
		registry.Each<Transform, Shape>(collectShape);

		if (hasAtlas)
		{
			const auto collectSprite =
			    [this, &place, &visible](ecs::Entity entity, const Transform& transform, const Sprite& sprite)
			{
				const Placement placement = place(entity, transform);
				const Position screen = Snap(placement.screen);
				// Its own size if it has one (stretched), else the picture's, magnified by the zoom.
				const auto width =
				    static_cast<float>((sprite.width > 0 ? sprite.width : sprite.source.width) * placement.zoom);
				const auto height =
				    static_cast<float>((sprite.height > 0 ? sprite.height : sprite.source.height) * placement.zoom);
				const Extent extent = Surround(screen, width, height, transform.rotation, transform.scale);
				if (!visible(placement, extent.topLeft, extent.width, extent.height)) return;
				entries.push_back({.screen = screen,
				                   .layer = sprite.layer,
				                   .color = sprite.tint,
				                   .clipped = placement.clipped,
				                   .clip = placement.clip,
				                   .textured = true,
				                   .source = sprite.source,
				                   .size = {width, height},
				                   .flipX = sprite.flipX,
				                   .rotation = transform.rotation,
				                   .scale = transform.scale});
			};
			registry.Each<Transform, Sprite>(collectSprite);
		}

		if (hasAtlas && settings.font != nullptr)
		{
			const BitmapFont& font = *settings.font;
			const auto collectText =
			    [this, &font, &place, &visible](ecs::Entity entity, const Transform& transform, const Text& text)
			{
				const Placement placement = place(entity, transform);
				// Zoom magnifies the text as a whole: its glyphs, spacing and wrap width.
				const int scale = std::max(text.scale, 1) * placement.zoom;
				glyphs.clear();
				LayoutText(font, text.value, Snap(placement.screen), scale, text.wrapWidth * placement.zoom, text.align,
				           glyphs);
				for (const GlyphQuad& glyph : glyphs)
				{
					const auto width = static_cast<float>(glyph.source.width * scale);
					const auto height = static_cast<float>(glyph.source.height * scale);
					if (!visible(placement, glyph.screen, width, height)) continue;
					entries.push_back({.screen = glyph.screen,
					                   .layer = text.layer,
					                   .color = text.color,
					                   .clipped = placement.clipped,
					                   .clip = placement.clip,
					                   .textured = true,
					                   .source = glyph.source,
					                   .size = {width, height}});
				}
			};
			registry.Each<Transform, Text>(collectText);
		}

		const auto byLayer = [](const Entry& a, const Entry& b)
		{
			return a.layer < b.layer;
		};
		std::ranges::stable_sort(entries, byLayer);

		// Shapes sample the middle of the solid pixel, so they come out in their own colour.
		const Position solidUv =
		    hasAtlas
		        ? Position{(static_cast<float>(settings.solid.x) + 0.5f) / static_cast<float>(settings.atlasWidth),
		                   (static_cast<float>(settings.solid.y) + 0.5f) / static_cast<float>(settings.atlasHeight)}
		        : Position{};

		for (Entry entry : entries)
		{
			const auto firstIndex = static_cast<int>(indices.size());
			if (entry.layer < settings.untintedFromLayer) entry.color = Multiply(entry.color, settings.tint);
			if (entry.textured)
			{
				AddQuad(entry, settings);
			}
			else if (entry.shape.kind == ShapeKind::Circle)
			{
				AddCircle(entry, solidUv);
			}
			else
			{
				AddRectangle(entry, solidUv);
			}

			// Entries drawn one after another inside the same clip share a run, so the renderer sets
			// the clip once for all of them.
			const bool sameRun = !runs.empty() && runs.back().clipped == entry.clipped &&
			                     (!entry.clipped || runs.back().clip == entry.clip);
			if (!sameRun) runs.push_back({.firstIndex = firstIndex, .clipped = entry.clipped, .clip = entry.clip});
			runs.back().indexCount += static_cast<int>(indices.size()) - firstIndex;
		}
	}

	void DrawBatch::AddRectangle(const Entry& entry, Position uv)
	{
		const int first = static_cast<int>(vertices.size());
		const float c = std::cos(Radians(entry.rotation));
		const float s = std::sin(Radians(entry.rotation));
		const float width = entry.shape.width;
		const float height = entry.shape.height;

		vertices.push_back({Corner(entry.screen, c, s, entry.scale, 0.0f, 0.0f), entry.color, uv});
		vertices.push_back({Corner(entry.screen, c, s, entry.scale, width, 0.0f), entry.color, uv});
		vertices.push_back({Corner(entry.screen, c, s, entry.scale, width, height), entry.color, uv});
		vertices.push_back({Corner(entry.screen, c, s, entry.scale, 0.0f, height), entry.color, uv});

		for (const int corner : {0, 1, 2, 0, 2, 3})
		{
			indices.push_back(first + corner);
		}
	}

	void DrawBatch::AddCircle(const Entry& entry, Position uv)
	{
		// A fan around the centre: one centre vertex, kCircleSegments rim vertices, and one
		// triangle per segment.
		const int centre = static_cast<int>(vertices.size());
		const float radius = entry.shape.radius;
		const float c = std::cos(Radians(entry.rotation));
		const float s = std::sin(Radians(entry.rotation));

		vertices.push_back({Corner(entry.screen, c, s, entry.scale, radius, radius), entry.color, uv});
		for (int i = 0; i < kCircleSegments; ++i)
		{
			const float angle =
			    2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(kCircleSegments);
			vertices.push_back({Corner(entry.screen, c, s, entry.scale, radius + radius * std::cos(angle),
			                           radius + radius * std::sin(angle)),
			                    entry.color, uv});
		}

		for (int i = 0; i < kCircleSegments; ++i)
		{
			indices.push_back(centre);
			indices.push_back(centre + 1 + i);
			indices.push_back(centre + 1 + (i + 1) % kCircleSegments);
		}
	}

	void DrawBatch::AddQuad(const Entry& entry, const BatchSettings& settings)
	{
		const int first = static_cast<int>(vertices.size());
		const float c = std::cos(Radians(entry.rotation));
		const float s = std::sin(Radians(entry.rotation));
		const float width = entry.size.x;
		const float height = entry.size.y;

		const auto atlasWidth = static_cast<float>(settings.atlasWidth);
		const auto atlasHeight = static_cast<float>(settings.atlasHeight);
		float u0 = static_cast<float>(entry.source.x) / atlasWidth;
		float u1 = static_cast<float>(entry.source.x + entry.source.width) / atlasWidth;
		const float v0 = static_cast<float>(entry.source.y) / atlasHeight;
		const float v1 = static_cast<float>(entry.source.y + entry.source.height) / atlasHeight;
		if (entry.flipX) std::swap(u0, u1);

		vertices.push_back({Corner(entry.screen, c, s, entry.scale, 0.0f, 0.0f), entry.color, {u0, v0}});
		vertices.push_back({Corner(entry.screen, c, s, entry.scale, width, 0.0f), entry.color, {u1, v0}});
		vertices.push_back({Corner(entry.screen, c, s, entry.scale, width, height), entry.color, {u1, v1}});
		vertices.push_back({Corner(entry.screen, c, s, entry.scale, 0.0f, height), entry.color, {u0, v1}});

		for (const int corner : {0, 1, 2, 0, 2, 3})
		{
			indices.push_back(first + corner);
		}
	}
} // namespace engine::graphics
