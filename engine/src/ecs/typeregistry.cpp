#include "ecs/typeregistry.hpp"

#include <charconv>
#include <cstdlib>

namespace engine::ecs
{
	namespace
	{
		std::string Child(const std::string& path, std::string_view key)
		{
			std::string result = path;
			result += '.';
			result += key;
			return result;
		}
	} // namespace

	namespace detail
	{
		double FloatToJsonDouble(float value)
		{
			// The shortest text that reads back as this float, read back as a double.
			char buffer[64];
			const auto result = std::to_chars(buffer, buffer + sizeof(buffer) - 1, value);
			if (result.ec != std::errc())
			{
				return static_cast<double>(value);
			}
			*result.ptr = '\0';
			return std::strtod(buffer, nullptr);
		}
	} // namespace detail

	bool FieldCodec<Entity>::Save(const Entity& value, const IEntityIds& ids, core::JsonValue& out, std::string& error)
	{
		if (value.IsNull())
		{
			out = core::JsonValue(nullptr);
			return true;
		}
		const auto id = ids.IdOf(value);
		if (!id)
		{
			error = "refers to an entity that is not part of the scene";
			return false;
		}
		out = core::JsonValue(*id);
		return true;
	}

	bool FieldCodec<Entity>::Load(Entity& value, const core::JsonValue& in, const IEntityIds& ids, std::string& error)
	{
		if (in.IsNull())
		{
			value = Entity::Null;
			return true;
		}
		const auto id = in.AsInt();
		if (!id)
		{
			error = detail::Expected("an entity id or null", in);
			return false;
		}
		const auto entity = ids.EntityOf(*id);
		if (!entity)
		{
			error = "no entity with id " + std::to_string(*id);
			return false;
		}
		value = *entity;
		return true;
	}

	bool FieldCodec<std::vector<Entity>>::Save(const std::vector<Entity>& value, const IEntityIds& ids,
	                                           core::JsonValue& out, std::string& error)
	{
		core::JsonValue list = core::JsonValue::Array();
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			core::JsonValue item;
			std::string itemError;
			if (!FieldCodec<Entity>::Save(value[i], ids, item, itemError))
			{
				error = "[" + std::to_string(i) + "] " + itemError;
				return false;
			}
			list.Append(std::move(item));
		}
		out = std::move(list);
		return true;
	}

	bool FieldCodec<std::vector<Entity>>::Load(std::vector<Entity>& value, const core::JsonValue& in,
	                                           const IEntityIds& ids, std::string& error)
	{
		const core::JsonArray* array = in.AsArray();
		if (array == nullptr)
		{
			error = detail::Expected("an array of entity ids", in);
			return false;
		}
		std::vector<Entity> loaded;
		for (std::size_t i = 0; i < array->size(); ++i)
		{
			Entity entity;
			std::string itemError;
			if (!FieldCodec<Entity>::Load(entity, (*array)[i], ids, itemError))
			{
				error = "[" + std::to_string(i) + "] " + itemError;
				return false;
			}
			loaded.push_back(entity);
		}
		value = std::move(loaded);
		return true;
	}

	bool TypeInfo::Save(const Registry& registry, Entity entity, const IEntityIds& ids, core::JsonValue& out,
	                    Issues& issues, const std::string& path) const
	{
		const void* component = Get(registry, entity);
		ENGINE_VERIFY(component != nullptr, "saving a component the entity does not have");
		core::JsonValue object = core::JsonValue::Object();
		bool ok = true;
		for (const FieldInfo& field : fields)
		{
			core::JsonValue value;
			std::string error;
			if (field.save(component, ids, value, error))
			{
				object.Set(field.name, std::move(value));
			}
			else
			{
				issues.errors.push_back({Child(path, field.name), error});
				ok = false;
			}
		}
		out = std::move(object);
		return ok;
	}

	bool TypeInfo::Load(Registry& registry, Entity entity, const core::JsonValue& in, const IEntityIds& ids,
	                    Issues& issues, const std::string& path) const
	{
		const core::JsonObject* object = in.AsObject();
		if (object == nullptr)
		{
			issues.errors.push_back({path, detail::Expected("an object", in)});
			return false;
		}
		const std::size_t errorsBefore = issues.errors.size();
		const bool added = addFilled(registry, entity, [&](void* component)
		{
			for (const auto& [key, value] : *object)
			{
				const FieldInfo* match = nullptr;
				for (const FieldInfo& field : fields)
				{
					if (field.name == key) match = &field;
				}
				if (match == nullptr)
				{
					issues.warnings.push_back(
					    {Child(path, key), std::string("unknown field of ") + name + ", ignored"});
					continue;
				}
				std::string error;
				if (!match->load(component, value, ids, error))
				{
					issues.errors.push_back({Child(path, key), error});
				}
			}
			return issues.errors.size() == errorsBefore;
		});
		return added;
	}

	TypeInfo& TypeRegistry::AddType(std::string_view name, std::type_index type)
	{
		ENGINE_VERIFY(!name.empty(), "a component needs a name");
		ENGINE_VERIFY(Find(name) == nullptr, "a component name registered twice");
		ENGINE_VERIFY(Find(type) == nullptr, "a component type registered twice");
		types.push_back(std::make_unique<TypeInfo>());
		TypeInfo& info = *types.back();
		info.name = std::string(name);
		info.type = type;
		return info;
	}

	const TypeInfo* TypeRegistry::Find(std::string_view name) const noexcept
	{
		for (const auto& info : types)
		{
			if (info->Name() == name) return info.get();
		}
		return nullptr;
	}

	const TypeInfo* TypeRegistry::Find(std::type_index type) const noexcept
	{
		for (const auto& info : types)
		{
			if (info->Type() == type) return info.get();
		}
		return nullptr;
	}

	const TypeInfo& TypeRegistry::At(std::size_t index) const
	{
		ENGINE_ASSERT(index < types.size(), "type index out of range");
		return *types[index];
	}
} // namespace engine::ecs
