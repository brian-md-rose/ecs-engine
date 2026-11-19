#include "ecs/scene.hpp"

#include <string>
#include <unordered_map>

#include "core/text.hpp"
#include "core/version.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/name.hpp"

namespace engine::ecs
{
	namespace
	{
		std::string Join(const std::string& path, std::string_view key)
		{
			std::string result = path;
			if (!result.empty()) result += '.';
			result += key;
			return result;
		}

		std::string Index(std::string_view array, std::size_t i)
		{
			std::string result(array);
			result += '[';
			result += std::to_string(i);
			result += ']';
			return result;
		}

		// The ids of the entities in a scene being written or read.
		class SceneIds final : public IEntityIds
		{
			std::unordered_map<Entity, std::int64_t> ids;
			std::unordered_map<std::int64_t, Entity> entities;

		public:
			// False if the id is already taken.
			bool Add(std::int64_t id, Entity entity)
			{
				if (!entities.emplace(id, entity).second) return false;
				ids.emplace(entity, id);
				return true;
			}

			[[nodiscard]] std::optional<std::int64_t> IdOf(Entity entity) const override
			{
				const auto it = ids.find(entity);
				if (it == ids.end()) return std::nullopt;
				return it->second;
			}

			[[nodiscard]] std::optional<Entity> EntityOf(std::int64_t id) const override
			{
				const auto it = entities.find(id);
				if (it == entities.end()) return std::nullopt;
				return it->second;
			}
		};

		core::JsonValue Save(const Registry& registry, const TypeRegistry& types, Issues& issues,
		                     std::span<const Entity> entities)
		{
			SceneIds ids;
			for (std::size_t i = 0; i < entities.size(); ++i)
			{
				ids.Add(static_cast<std::int64_t>(i) + 1, entities[i]);
			}

			core::JsonValue list = core::JsonValue::Array();
			for (std::size_t i = 0; i < entities.size(); ++i)
			{
				const Entity entity = entities[i];
				const std::string path = Index("entities", i);
				core::JsonValue object = core::JsonValue::Object();
				object.Set("id", static_cast<std::int64_t>(i) + 1);
				if (const Name* name = registry.TryGetComponent<Name>(entity))
				{
					object.Set("name", name->value);
				}
				if (const Parent* parent = registry.TryGetComponent<Parent>(entity))
				{
					const auto parentId = ids.IdOf(parent->entity);
					if (parentId)
					{
						object.Set("parent", *parentId);
					}
					else if (!parent->entity.IsNull())
					{
						issues.errors.push_back({Join(path, "parent"), "the parent is not part of the scene"});
					}
				}

				core::JsonValue components = core::JsonValue::Object();
				for (std::size_t t = 0; t < types.Count(); ++t)
				{
					const TypeInfo& type = types.At(t);
					if (type.IsRuntime() || type.Type() == std::type_index(typeid(Name)) ||
					    type.Type() == std::type_index(typeid(Parent)) || !type.Has(registry, entity))
					{
						continue;
					}
					core::JsonValue saved;
					type.Save(registry, entity, ids, saved, issues, Join(Join(path, "components"), type.Name()));
					components.Set(type.Name(), std::move(saved));
				}
				object.Set("components", std::move(components));
				list.Append(std::move(object));
			}

			core::JsonValue scene = core::JsonValue::Object();
			scene.Set("format", kSceneFormat);
			scene.Set("engine", kVersionString);
			scene.Set("entities", std::move(list));
			return scene;
		}
	} // namespace

	core::JsonValue SaveScene(const Registry& registry, const TypeRegistry& types, Issues& issues)
	{
		const std::vector<Entity> all = registry.AllEntities();
		return Save(registry, types, issues, all);
	}

	core::JsonValue SaveScene(const Registry& registry, const TypeRegistry& types, Issues& issues,
	                          std::span<const Entity> entities)
	{
		return Save(registry, types, issues, entities);
	}

	SceneLoadResult LoadScene(Registry& registry, const TypeRegistry& types, const core::JsonValue& scene)
	{
		SceneLoadResult result;
		Issues& issues = result.issues;

		const core::JsonObject* root = scene.AsObject();
		if (root == nullptr)
		{
			issues.errors.push_back({"", "a scene is a JSON object"});
			return result;
		}
		const core::JsonValue* format = scene.Find("format");
		const auto formatNumber = format != nullptr ? format->AsInt() : std::nullopt;
		if (!formatNumber)
		{
			issues.errors.push_back({"format", "missing or not an integer"});
			return result;
		}
		if (*formatNumber != kSceneFormat)
		{
			// Older formats would be migrated here, one step at a time, up to kSceneFormat.
			issues.errors.push_back({"format", "scene format " + std::to_string(*formatNumber) +
			                                       " is not supported; this engine reads format " +
			                                       std::to_string(kSceneFormat)});
			return result;
		}
		const core::JsonValue* entitiesJson = scene.Find("entities");
		const core::JsonArray* list = entitiesJson != nullptr ? entitiesJson->AsArray() : nullptr;
		if (list == nullptr)
		{
			issues.errors.push_back({"entities", "missing or not an array"});
			return result;
		}

		// Pass 1: create every entity and note its id, so references can point forwards or backwards.
		SceneIds ids;
		std::vector<Entity> created;
		for (std::size_t i = 0; i < list->size(); ++i)
		{
			const std::string path = Index("entities", i);
			const core::JsonValue& item = (*list)[i];
			const core::JsonValue* idJson = item.Find("id");
			const auto id = idJson != nullptr ? idJson->AsInt() : std::nullopt;
			const Entity entity = registry.Create();
			created.push_back(entity);
			if (!id)
			{
				issues.errors.push_back({Join(path, "id"), "missing or not an integer"});
			}
			else if (!ids.Add(*id, entity))
			{
				issues.errors.push_back({Join(path, "id"), "the id " + std::to_string(*id) + " is used twice"});
			}
		}

		// Pass 2: names, parents and components.
		for (std::size_t i = 0; i < list->size(); ++i)
		{
			const std::string path = Index("entities", i);
			const core::JsonValue& item = (*list)[i];
			const Entity entity = created[i];
			if (!item.IsObject())
			{
				issues.errors.push_back({path, "expected an object"});
				continue;
			}
			for (const auto& [key, value] : *item.AsObject())
			{
				if (key != "id" && key != "name" && key != "parent" && key != "components")
				{
					issues.warnings.push_back({Join(path, key), "unknown key, ignored"});
				}
			}
			if (const core::JsonValue* name = item.Find("name"))
			{
				const auto text = name->AsString();
				if (text)
				{
					registry.Add<Name>(entity, Name{std::string(*text)});
				}
				else
				{
					issues.errors.push_back({Join(path, "name"), "expected a string"});
				}
			}
			if (const core::JsonValue* parent = item.Find("parent"))
			{
				const auto parentId = parent->AsInt();
				const auto parentEntity = parentId ? ids.EntityOf(*parentId) : std::nullopt;
				if (parentEntity)
				{
					registry.Add<Parent>(entity, Parent{*parentEntity});
				}
				else
				{
					issues.errors.push_back(
					    {Join(path, "parent"),
					     parentId ? "no entity with id " + std::to_string(*parentId) : "expected an entity id"});
				}
			}
			const core::JsonValue* components = item.Find("components");
			if (components == nullptr) continue;
			if (!components->IsObject())
			{
				issues.errors.push_back({Join(path, "components"), "expected an object"});
				continue;
			}
			for (const auto& [typeName, body] : *components->AsObject())
			{
				const std::string componentPath = Join(Join(path, "components"), typeName);
				const TypeInfo* type = types.Find(typeName);
				if (type == nullptr || type->IsRuntime())
				{
					issues.warnings.push_back({componentPath, "unknown component, ignored"});
					continue;
				}
				type->Load(registry, entity, body, ids, issues, componentPath);
			}
		}

		if (!issues.Ok())
		{
			for (const Entity entity : created)
			{
				registry.Destroy(entity);
			}
			return result;
		}
		result.entities = std::move(created);
		return result;
	}

	SceneLoadResult LoadSceneFile(Registry& registry, const TypeRegistry& types, const std::filesystem::path& file)
	{
		const core::JsonResult parsed = core::LoadJson(file);
		if (!parsed.value)
		{
			SceneLoadResult result;
			result.issues.errors.push_back({"", core::FormatError(file.string(), parsed.error)});
			return result;
		}
		return LoadScene(registry, types, *parsed.value);
	}

	bool SaveSceneFile(const Registry& registry, const TypeRegistry& types, const std::filesystem::path& file,
	                   Issues& issues)
	{
		const core::JsonValue scene = SaveScene(registry, types, issues);
		if (!issues.Ok()) return false;
		if (!core::WriteJson(file, scene))
		{
			issues.errors.push_back({"", "could not write " + file.string()});
			return false;
		}
		return true;
	}
} // namespace engine::ecs
