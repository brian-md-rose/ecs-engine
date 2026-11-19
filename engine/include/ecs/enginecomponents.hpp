#pragma once
#include <string>

#include "core/json.hpp"
#include "ecs/typeregistry.hpp"
#include "graphics/color.hpp"
#include "graphics/image.hpp"
#include "graphics/transform.hpp"

// Registers the engine's own components (Name, Parent, Position-based transforms, Shape, Sprite,
// ClipRect, ScreenSpace) with a TypeRegistry, and defines the FieldCodecs for the engine value
// types a game's own components are likely to hold: Color, Position and PixelRect.
namespace engine::ecs
{
	// A Color is written as [r, g, b].
	template <> struct FieldCodec<graphics::Color>
	{
		static constexpr FieldKind kind = FieldKind::Custom;
		static bool Save(const graphics::Color& value, const IEntityIds&, core::JsonValue& out, std::string& error);
		static bool Load(graphics::Color& value, const core::JsonValue& in, const IEntityIds&, std::string& error);
	};

	// A Position is written as {"x": ..., "y": ...}.
	template <> struct FieldCodec<graphics::Position>
	{
		static constexpr FieldKind kind = FieldKind::Custom;
		static bool Save(const graphics::Position& value, const IEntityIds&, core::JsonValue& out, std::string& error);
		static bool Load(graphics::Position& value, const core::JsonValue& in, const IEntityIds&, std::string& error);
	};

	// A PixelRect is written as {"x": ..., "y": ..., "width": ..., "height": ...}.
	template <> struct FieldCodec<graphics::PixelRect>
	{
		static constexpr FieldKind kind = FieldKind::Custom;
		static bool Save(const graphics::PixelRect& value, const IEntityIds&, core::JsonValue& out, std::string& error);
		static bool Load(graphics::PixelRect& value, const core::JsonValue& in, const IEntityIds&, std::string& error);
	};

	// Registers Name, Parent, Transform, LocalTransform, Shape, Sprite, ClipRect and ScreenSpace,
	// and Interpolation as a Runtime component. Call it once, before a game's own registrations.
	void RegisterEngineComponents(TypeRegistry& types);
} // namespace engine::ecs
