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

		// A parent chain that comes back to its start would make anything that walks the hierarchy loop.
		if (issues.Ok())
		{
			for (std::size_t i = 0; i < created.size(); ++i)
			{
				Entity current = created[i];
				for (std::size_t step = 0; step <= created.size(); ++step)
				{
					const Parent* parent = registry.TryGetComponent<Parent>(current);
					if (parent == nullptr || parent->entity.IsNull()) break;
					current = parent->entity;
					if (step == created.size())
					{
						issues.errors.push_back({Join(Index("entities", i), "parent"), "the parents form a loop"});
					}
				}
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

	SceneSettings ReadSceneSettings(const core::JsonValue& scene, Issues& issues)
	{
		SceneSettings settings;
		const core::JsonValue* json = scene.Find("settings");
		if (json == nullptr) return settings;
		if (json->AsObject() == nullptr)
		{
			issues.warnings.push_back({"settings", "not an object, ignored"});
			return settings;
		}
		if (const core::JsonValue* camera = json->Find("camera"))
		{
			const core::JsonValue* x = camera->Find("x");
			const core::JsonValue* y = camera->Find("y");
			const core::JsonValue* zoom = camera->Find("zoom");
			const auto number = [](const core::JsonValue* value) -> std::optional<double>
			{
				if (value == nullptr) return std::nullopt;
				if (const auto whole = value->AsInt()) return static_cast<double>(*whole);
				return value->AsDouble();
			};
			const auto cx = number(x);
			const auto cy = number(y);
			const auto cz = zoom != nullptr ? zoom->AsInt() : std::optional<std::int64_t>(1);
			if (!cx || !cy || !cz || *cz < 1 || *cz > 64)
				issues.warnings.push_back(
				    {"settings.camera", "needs numbers x and y and a zoom from 1 to 64, ignored"});
			else
				settings.camera = SceneCamera{static_cast<float>(*cx), static_cast<float>(*cy), static_cast<int>(*cz)};
		}
		if (const core::JsonValue* background = json->Find("background"))
		{
			const core::JsonArray* parts = background->AsArray();
			std::array<std::uint8_t, 3> color{};
			bool good = parts != nullptr && parts->size() == 3;
			for (std::size_t i = 0; good && i < 3; ++i)
			{
				const auto channel = (*parts)[i].AsInt();
				good = channel && *channel >= 0 && *channel <= 255;
				if (good) color[i] = static_cast<std::uint8_t>(*channel);
			}
			if (good)
				settings.background = color;
			else
				issues.warnings.push_back({"settings.background", "needs three numbers from 0 to 255, ignored"});
		}
		return settings;
	}

	void WriteSceneSettings(core::JsonValue& scene, const SceneSettings& settings)
	{
		const core::JsonObject* members = scene.AsObject();
		if (members == nullptr) return;
		if (scene.Find("settings") != nullptr)
		{
			core::JsonObject kept;
			for (const auto& member : *members)
				if (member.first != "settings") kept.push_back(member);
			scene = core::JsonValue(std::move(kept));
		}
		if (settings.Empty()) return;
		core::JsonValue json = core::JsonValue::Object();
		if (settings.camera)
		{
			core::JsonValue camera = core::JsonValue::Object();
			camera.Set("x", core::JsonValue(static_cast<double>(settings.camera->x)));
			camera.Set("y", core::JsonValue(static_cast<double>(settings.camera->y)));
			camera.Set("zoom", core::JsonValue(static_cast<std::int64_t>(settings.camera->zoom)));
			json.Set("camera", std::move(camera));
		}
		if (settings.background)
		{
			core::JsonValue color = core::JsonValue::Array();
			for (const std::uint8_t channel : *settings.background)
				color.Append(core::JsonValue(static_cast<std::int64_t>(channel)));
			json.Set("background", std::move(color));
		}
		scene.Set("settings", std::move(json));
	}

	SceneLoadResult LoadSceneFile(Registry& registry, const TypeRegistry& types, const std::filesystem::path& file,
	                              SceneSettings* settings)
	{
		if (settings != nullptr) *settings = SceneSettings{};
		const core::JsonResult parsed = core::LoadJson(file);
		if (!parsed.value)
		{
			SceneLoadResult result;
			result.issues.errors.push_back({"", core::FormatError(file.string(), parsed.error)});
			return result;
		}
		SceneLoadResult result = LoadScene(registry, types, *parsed.value);
		if (settings != nullptr && result.Ok()) *settings = ReadSceneSettings(*parsed.value, result.issues);
		return result;
	}

	bool SaveSceneFile(const Registry& registry, const TypeRegistry& types, const std::filesystem::path& file,
	                   Issues& issues, const SceneSettings* settings)
	{
		const std::size_t errorsBefore = issues.errors.size();
		core::JsonValue scene = SaveScene(registry, types, issues);
		if (settings != nullptr) WriteSceneSettings(scene, *settings);
		if (issues.errors.size() != errorsBefore) return false;
		if (!core::WriteJson(file, scene))
		{
			issues.errors.push_back({"", "could not write " + file.string()});
			return false;
		}
		return true;
	}
} // namespace engine::ecs
