#include "ecs/enginecomponents.hpp"

#include <array>
#include <cstdint>

#include "ecs/hierarchy.hpp"
#include "ecs/name.hpp"
#include "graphics/clip.hpp"
#include "graphics/font.hpp"
#include "graphics/interpolation.hpp"
#include "graphics/screenspace.hpp"
#include "graphics/shape.hpp"
#include "graphics/sprite.hpp"
#include "ui/ninepatch.hpp"
#include "ui/ui.hpp"

namespace engine::ecs
{
	namespace
	{
		// Reads member key of an object into target through its codec, naming the key in an error.
		template <typename T>
		bool LoadMember(const core::JsonValue& object, std::string_view key, T& target, const IEntityIds& ids,
		                std::string& error)
		{
			const core::JsonValue* member = object.Find(key);
			if (member == nullptr)
			{
				return true; // missing keeps the default
			}
			std::string memberError;
			if (!FieldCodec<T>::Load(target, *member, ids, memberError))
			{
				error = "." + std::string(key) + " " + memberError;
				return false;
			}
			return true;
		}

		bool ExpectObject(const core::JsonValue& in, std::string& error)
		{
			if (!in.IsObject())
			{
				error = detail::Expected("an object", in);
				return false;
			}
			return true;
		}
	} // namespace

	bool FieldCodec<graphics::Color>::Save(const graphics::Color& value, const IEntityIds&, core::JsonValue& out,
	                                       std::string&)
	{
		core::JsonValue array = core::JsonValue::Array();
		array.Append(value.r);
		array.Append(value.g);
		array.Append(value.b);
		out = std::move(array);
		return true;
	}

	bool FieldCodec<graphics::Color>::Load(graphics::Color& value, const core::JsonValue& in, const IEntityIds& ids,
	                                       std::string& error)
	{
		const core::JsonArray* array = in.AsArray();
		if (array == nullptr || array->size() != 3)
		{
			error = "expected an array of 3 numbers [r, g, b]";
			return false;
		}
		graphics::Color loaded;
		std::array<std::uint8_t*, 3> channels = {&loaded.r, &loaded.g, &loaded.b};
		for (std::size_t i = 0; i < channels.size(); ++i)
		{
			std::string channelError;
			if (!FieldCodec<std::uint8_t>::Load(*channels[i], (*array)[i], ids, channelError))
			{
				error = "[" + std::to_string(i) + "] " + channelError;
				return false;
			}
		}
		value = loaded;
		return true;
	}

	bool FieldCodec<graphics::Position>::Save(const graphics::Position& value, const IEntityIds& ids,
	                                          core::JsonValue& out, std::string& error)
	{
		core::JsonValue object = core::JsonValue::Object();
		core::JsonValue x;
		core::JsonValue y;
		if (!FieldCodec<float>::Save(value.x, ids, x, error) || !FieldCodec<float>::Save(value.y, ids, y, error))
		{
			return false;
		}
		object.Set("x", std::move(x));
		object.Set("y", std::move(y));
		out = std::move(object);
		return true;
	}

	bool FieldCodec<graphics::Position>::Load(graphics::Position& value, const core::JsonValue& in,
	                                          const IEntityIds& ids, std::string& error)
	{
		if (!ExpectObject(in, error)) return false;
		graphics::Position loaded = value;
		if (!LoadMember(in, "x", loaded.x, ids, error) || !LoadMember(in, "y", loaded.y, ids, error)) return false;
		value = loaded;
		return true;
	}

	bool FieldCodec<graphics::Scale>::Save(const graphics::Scale& value, const IEntityIds& ids, core::JsonValue& out,
	                                       std::string& error)
	{
		core::JsonValue object = core::JsonValue::Object();
		core::JsonValue x;
		core::JsonValue y;
		if (!FieldCodec<float>::Save(value.x, ids, x, error) || !FieldCodec<float>::Save(value.y, ids, y, error))
		{
			return false;
		}
		object.Set("x", std::move(x));
		object.Set("y", std::move(y));
		out = std::move(object);
		return true;
	}

	bool FieldCodec<graphics::Scale>::Load(graphics::Scale& value, const core::JsonValue& in, const IEntityIds& ids,
	                                       std::string& error)
	{
		if (!ExpectObject(in, error)) return false;
		graphics::Scale loaded = value;
		if (!LoadMember(in, "x", loaded.x, ids, error) || !LoadMember(in, "y", loaded.y, ids, error)) return false;
		value = loaded;
		return true;
	}

	bool FieldCodec<graphics::PixelRect>::Save(const graphics::PixelRect& value, const IEntityIds& ids,
	                                           core::JsonValue& out, std::string& error)
	{
		core::JsonValue object = core::JsonValue::Object();
		const std::array<std::pair<const char*, int>, 4> members = {
		    {{"x", value.x}, {"y", value.y}, {"width", value.width}, {"height", value.height}}};
		for (const auto& [key, number] : members)
		{
			core::JsonValue json;
			if (!FieldCodec<int>::Save(number, ids, json, error)) return false;
			object.Set(key, std::move(json));
		}
		out = std::move(object);
		return true;
	}

	bool FieldCodec<graphics::PixelRect>::Load(graphics::PixelRect& value, const core::JsonValue& in,
	                                           const IEntityIds& ids, std::string& error)
	{
		if (!ExpectObject(in, error)) return false;
		graphics::PixelRect loaded = value;
		if (!LoadMember(in, "x", loaded.x, ids, error) || !LoadMember(in, "y", loaded.y, ids, error) ||
		    !LoadMember(in, "width", loaded.width, ids, error) || !LoadMember(in, "height", loaded.height, ids, error))
		{
			return false;
		}
		value = loaded;
		return true;
	}

	void RegisterEngineComponents(TypeRegistry& types)
	{
		using namespace graphics;
		types.Register<Name>("Name").Field("value", &Name::value);
		types.Register<Parent>("Parent").Field("entity", &Parent::entity);
		types.Register<Transform>("Transform")
		    .Field("position", &Transform::position)
		    .Field("rotation", &Transform::rotation, {.tooltip = "degrees, clockwise"})
		    .Field("scale", &Transform::scale);
		types.Register<LocalTransform>("LocalTransform")
		    .Field("position", &LocalTransform::position)
		    .Field("rotation", &LocalTransform::rotation, {.tooltip = "degrees, clockwise"})
		    .Field("scale", &LocalTransform::scale);
		types.Register<Shape>("Shape")
		    .EnumField("kind", &Shape::kind, {{"Rectangle", ShapeKind::Rectangle}, {"Circle", ShapeKind::Circle}})
		    .Field("width", &Shape::width)
		    .Field("height", &Shape::height)
		    .Field("radius", &Shape::radius)
		    .Field("color", &Shape::color)
		    .Field("layer", &Shape::layer);
		types.Register<Sprite>("Sprite")
		    .Field("source", &Sprite::source,
		           {.tooltip = "the part of the atlas to draw; an editor can pick it by sprite name",
		            .assetKind = AssetKind::Sprite})
		    .Field("tint", &Sprite::tint)
		    .Field("layer", &Sprite::layer)
		    .Field("flipX", &Sprite::flipX)
		    .Field("width", &Sprite::width)
		    .Field("height", &Sprite::height);
		types.Register<ClipRect>("ClipRect").Field("rect", &ClipRect::rect);
		types.RegisterTag<ScreenSpace>("ScreenSpace");
		types.Register<Text>("Text")
		    .Field("value", &Text::value)
		    .Field("color", &Text::color)
		    .Field("layer", &Text::layer)
		    .Field("scale", &Text::scale)
		    .Field("wrapWidth", &Text::wrapWidth)
		    .EnumField("align", &Text::align,
		               {{"Left", Align::Left}, {"Centre", Align::Centre}, {"Right", Align::Right}});
		types.Register<ui::NinePatch>("NinePatch")
		    .Field("source", &ui::NinePatch::source)
		    .Field("left", &ui::NinePatch::left)
		    .Field("right", &ui::NinePatch::right)
		    .Field("top", &ui::NinePatch::top)
		    .Field("bottom", &ui::NinePatch::bottom)
		    .Field("width", &ui::NinePatch::width)
		    .Field("height", &ui::NinePatch::height)
		    .Field("tint", &ui::NinePatch::tint)
		    .Field("layer", &ui::NinePatch::layer);
		types.Register<ui::Hitbox>("Hitbox")
		    .Field("width", &ui::Hitbox::width)
		    .Field("height", &ui::Hitbox::height)
		    .Field("layer", &ui::Hitbox::layer)
		    .Field("blocksBelow", &ui::Hitbox::blocksBelow);
		types.Register<ui::Interactable>("Interactable").Field("enabled", &ui::Interactable::enabled);
		types.Register<ui::FocusOrder>("FocusOrder").Field("order", &ui::FocusOrder::order);
		types.Register<ui::ScrollView>("ScrollView")
		    .Field("content", &ui::ScrollView::content)
		    .Field("viewHeight", &ui::ScrollView::viewHeight)
		    .Field("contentHeight", &ui::ScrollView::contentHeight)
		    .Field("offset", &ui::ScrollView::offset)
		    .Field("wheelStep", &ui::ScrollView::wheelStep)
		    .Field("dragThreshold", &ui::ScrollView::dragThreshold)
		    .Field("padSpeed", &ui::ScrollView::padSpeed);
		types.Register<Interpolation>("Interpolation").Runtime();
	}
} // namespace engine::ecs
