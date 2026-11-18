#include "graphics/drawbatch.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "graphics/interpolation.hpp"
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

		[[nodiscard]] Position ToScreen(Position world, const Camera& camera)
		{
			return {world.x - camera.position.x, world.y - camera.position.y};
		}

		[[nodiscard]] Position Snap(Position position)
		{
			return {std::floor(position.x + 0.5f), std::floor(position.y + 0.5f)};
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

		const auto collectShape =
		    [this, &camera, &worldPosition](ecs::Entity entity, const Transform& transform, const Shape& shape)
		{
			const Position screen = ToScreen(worldPosition(entity, transform), camera);
			if (!Overlaps(screen, shape.BoundsWidth(), shape.BoundsHeight(), camera)) return;
			entries.push_back({.screen = screen, .layer = shape.layer, .color = shape.color, .shape = shape});
		};
		registry.Each<Transform, Shape>(collectShape);

		if (hasAtlas)
		{
			const auto collectSprite =
			    [this, &camera, &worldPosition](ecs::Entity entity, const Transform& transform, const Sprite& sprite)
			{
				const Position screen = Snap(ToScreen(worldPosition(entity, transform), camera));
				const auto width = static_cast<float>(sprite.source.width);
				const auto height = static_cast<float>(sprite.source.height);
				if (!Overlaps(screen, width, height, camera)) return;
				entries.push_back({.screen = screen,
				                   .layer = sprite.layer,
				                   .color = sprite.tint,
				                   .textured = true,
				                   .source = sprite.source,
				                   .flipX = sprite.flipX});
			};
			registry.Each<Transform, Sprite>(collectSprite);
		}

		if (hasAtlas && settings.font != nullptr)
		{
			const BitmapFont& font = *settings.font;
			const auto collectText =
			    [this, &camera, &font, &worldPosition](ecs::Entity entity, const Transform& transform, const Text& text)
			{
				const int scale = std::max(text.scale, 1);
				glyphs.clear();
				LayoutText(font, text.value, Snap(ToScreen(worldPosition(entity, transform), camera)), scale,
				           text.wrapWidth, text.align, glyphs);
				for (const GlyphQuad& glyph : glyphs)
				{
					const auto width = static_cast<float>(glyph.source.width * scale);
					const auto height = static_cast<float>(glyph.source.height * scale);
					if (!Overlaps(glyph.screen, width, height, camera)) continue;
					entries.push_back({.screen = glyph.screen,
					                   .layer = text.layer,
					                   .color = text.color,
					                   .textured = true,
					                   .source = glyph.source,
					                   .scale = scale});
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
		}
	}

	void DrawBatch::AddRectangle(const Entry& entry, Position uv)
	{
		const int first = static_cast<int>(vertices.size());
		const float left = entry.screen.x;
		const float top = entry.screen.y;
		const float right = left + entry.shape.width;
		const float bottom = top + entry.shape.height;

		vertices.push_back({{left, top}, entry.color, uv});
		vertices.push_back({{right, top}, entry.color, uv});
		vertices.push_back({{right, bottom}, entry.color, uv});
		vertices.push_back({{left, bottom}, entry.color, uv});

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
		const float cx = entry.screen.x + radius;
		const float cy = entry.screen.y + radius;

		vertices.push_back({{cx, cy}, entry.color, uv});
		for (int i = 0; i < kCircleSegments; ++i)
		{
			const float angle =
			    2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(kCircleSegments);
			vertices.push_back({{cx + radius * std::cos(angle), cy + radius * std::sin(angle)}, entry.color, uv});
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
		const float left = entry.screen.x;
		const float top = entry.screen.y;
		const float right = left + static_cast<float>(entry.source.width * entry.scale);
		const float bottom = top + static_cast<float>(entry.source.height * entry.scale);

		const auto atlasWidth = static_cast<float>(settings.atlasWidth);
		const auto atlasHeight = static_cast<float>(settings.atlasHeight);
		float u0 = static_cast<float>(entry.source.x) / atlasWidth;
		float u1 = static_cast<float>(entry.source.x + entry.source.width) / atlasWidth;
		const float v0 = static_cast<float>(entry.source.y) / atlasHeight;
		const float v1 = static_cast<float>(entry.source.y + entry.source.height) / atlasHeight;
		if (entry.flipX) std::swap(u0, u1);

		vertices.push_back({{left, top}, entry.color, {u0, v0}});
		vertices.push_back({{right, top}, entry.color, {u1, v0}});
		vertices.push_back({{right, bottom}, entry.color, {u1, v1}});
		vertices.push_back({{left, bottom}, entry.color, {u0, v1}});

		for (const int corner : {0, 1, 2, 0, 2, 3})
		{
			indices.push_back(first + corner);
		}
	}
} // namespace engine::graphics
