#include "editor/entitypanels.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/json.hpp"
#include "ecs/hierarchy.hpp"
#include "ecs/name.hpp"
#include "ecs/typeregistry.hpp"
#include "editor/entityops.hpp"

namespace engine::editor
{
	namespace
	{
		using core::JsonArray;
		using core::JsonValue;
		using ecs::Entity;
		using ecs::FieldInfo;
		using ecs::FieldKind;
		using ecs::Registry;
		using ecs::TypeInfo;
		using ecs::TypeRegistry;

		constexpr const char* kDragPayload = "ecs-entity";
		constexpr std::size_t kMaxJsonDepth = 6;

		// What the hierarchy was asked to do while it was drawing; done once the tree is finished, since drawing
		// walks the entities it would change.
		struct Pending
		{
			enum class Kind
			{
				None,
				AddChild,
				Duplicate,
				Delete,
				Reparent
			};
			Kind kind = Kind::None;
			Entity target = Entity::Null;
			Entity newParent = Entity::Null;
		};

		void DrawNode(Registry& registry, const Tree& tree, PanelState& state, Pending& pending, Entity entity,
		              std::vector<Entity>& path)
		{
			if (!SubtreeMatches(registry, tree, entity, state.filter)) return;
			const auto found = tree.children.find(entity);
			const bool hasChildren = found != tree.children.end();

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
			if (entity == state.selected) flags |= ImGuiTreeNodeFlags_Selected;
			if (!state.filter.empty()) flags |= ImGuiTreeNodeFlags_DefaultOpen;

			ImGui::PushID(static_cast<int>(entity.id));
			const bool open = ImGui::TreeNodeEx(LabelOf(registry, entity).c_str(), flags);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) state.selected = entity;

			if (ImGui::BeginPopupContextItem())
			{
				state.selected = entity;
				if (ImGui::MenuItem("Add child")) pending = {Pending::Kind::AddChild, entity, Entity::Null};
				if (ImGui::MenuItem("Duplicate")) pending = {Pending::Kind::Duplicate, entity, Entity::Null};
				if (ImGui::MenuItem("Delete")) pending = {Pending::Kind::Delete, entity, Entity::Null};
				ImGui::EndPopup();
			}
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(kDragPayload, &entity, sizeof(Entity));
				ImGui::TextUnformatted(LabelOf(registry, entity).c_str());
				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragPayload))
				{
					Entity dropped{};
					std::memcpy(&dropped, payload->Data, sizeof(Entity));
					pending = {Pending::Kind::Reparent, dropped, entity};
				}
				ImGui::EndDragDropTarget();
			}

			if (open)
			{
				if (hasChildren)
				{
					// The path stops a Parent loop from drawing the same entities forever.
					path.push_back(entity);
					for (const Entity child : found->second)
					{
						if (std::find(path.begin(), path.end(), child) == path.end())
						{
							DrawNode(registry, tree, state, pending, child, path);
						}
					}
					path.pop_back();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		void Apply(Registry& registry, const TypeRegistry* types, PanelState& state, const Pending& pending)
		{
			switch (pending.kind)
			{
			case Pending::Kind::None:
				break;
			case Pending::Kind::AddChild:
				state.selected = CreateEntity(registry, "New Entity", pending.target);
				break;
			case Pending::Kind::Duplicate:
			{
				if (types == nullptr) break;
				ecs::Issues issues;
				const Entity copy = DuplicateTree(registry, *types, pending.target, issues);
				if (!copy.IsNull())
				{
					state.selected = copy;
				}
				else if (!issues.errors.empty())
				{
					state.status =
					    "can't duplicate: " + issues.errors.front().path + ": " + issues.errors.front().message;
				}
				break;
			}
			case Pending::Kind::Delete:
				DestroyTree(registry, pending.target);
				if (!registry.IsAlive(state.selected)) state.selected = Entity::Null;
				break;
			case Pending::Kind::Reparent:
				if (!SetParent(registry, pending.target, pending.newParent))
				{
					state.status = "can't make " + LabelOf(registry, pending.target) + " a child of " +
					               LabelOf(registry, pending.newParent) + " (it would be its own ancestor)";
				}
				break;
			}
		}

		void DrawHierarchy(ecs::World& world, PanelState& state)
		{
			Registry& registry = world.registry;
			const TypeRegistry* types = world.resources.TryGet<TypeRegistry>();
			if (types == nullptr) ImGui::TextDisabled("No TypeRegistry resource: copies and the inspector are off.");

			if (ImGui::Button("New entity")) state.selected = CreateEntity(registry, "New Entity");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##filter", "filter by name", &state.filter);
			if (!state.status.empty())
			{
				ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "%s", state.status.c_str());
			}
			ImGui::TextDisabled("%zu entities", registry.EntityCount());
			ImGui::Separator();

			const Tree tree = BuildTree(registry);
			Pending pending;
			if (ImGui::BeginChild("tree"))
			{
				std::vector<Entity> path;
				for (const Entity root : tree.roots)
					DrawNode(registry, tree, state, pending, root, path);
				ImGui::Spacing();
				ImGui::TextDisabled("(drop here for the top level)");
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragPayload))
					{
						Entity dropped{};
						std::memcpy(&dropped, payload->Data, sizeof(Entity));
						pending = {Pending::Kind::Reparent, dropped, Entity::Null};
					}
					ImGui::EndDragDropTarget();
				}
			}
			ImGui::EndChild();
			if (pending.kind != Pending::Kind::None) state.status.clear();
			Apply(registry, types, state, pending);
		}

		// ---- the inspector ----

		// Picks an entity (or none) from the live entities. Returns true and the new id when the choice changed.
		bool EntityCombo(const char* label, const Registry& registry, const std::vector<Entity>& all,
		                 std::optional<std::int64_t>& id)
		{
			std::string current = "(none)";
			if (id)
			{
				current = "(missing)";
				for (const Entity entity : all)
				{
					if (static_cast<std::int64_t>(entity.id) == *id) current = LabelOf(registry, entity);
				}
			}
			bool changed = false;
			if (ImGui::BeginCombo(label, current.c_str()))
			{
				if (ImGui::Selectable("(none)", !id))
				{
					id.reset();
					changed = true;
				}
				for (const Entity entity : all)
				{
					const auto entityId = static_cast<std::int64_t>(entity.id);
					ImGui::PushID(static_cast<int>(entity.id));
					if (ImGui::Selectable(LabelOf(registry, entity).c_str(), id && *id == entityId))
					{
						id = entityId;
						changed = true;
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			return changed;
		}

		// Edits any JSON value in place: numbers as drag boxes, bool, text, and arrays and objects of those.
		// A fallback for a field with a codec of its own, such as a Color or a Position.
		bool EditJson(const char* label, JsonValue& value, std::size_t depth)
		{
			if (depth > kMaxJsonDepth)
			{
				ImGui::TextDisabled("%s: (too deep)", label);
				return false;
			}
			if (const auto flag = value.AsBool())
			{
				bool edited = *flag;
				if (!ImGui::Checkbox(label, &edited)) return false;
				value = JsonValue(edited);
				return true;
			}
			if (value.IsInt())
			{
				std::int64_t number = *value.AsInt();
				if (!ImGui::DragScalar(label, ImGuiDataType_S64, &number, 1.0F)) return false;
				value = JsonValue(number);
				return true;
			}
			if (value.IsDouble())
			{
				double number = *value.AsDouble();
				if (!ImGui::DragScalar(label, ImGuiDataType_Double, &number, 0.05F)) return false;
				value = JsonValue(number);
				return true;
			}
			if (const auto text = value.AsString())
			{
				std::string edited(*text);
				if (!ImGui::InputText(label, &edited)) return false;
				value = JsonValue(std::move(edited));
				return true;
			}
			if (const JsonArray* items = value.AsArray())
			{
				JsonArray edited = *items;
				bool changed = false;
				if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (std::size_t i = 0; i < edited.size(); ++i)
					{
						ImGui::PushID(static_cast<int>(i));
						changed |= EditJson(std::to_string(i).c_str(), edited[i], depth + 1);
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
				if (changed) value = JsonValue(std::move(edited));
				return changed;
			}
			if (const core::JsonObject* members = value.AsObject())
			{
				core::JsonObject edited = *members;
				bool changed = false;
				if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (auto& [key, member] : edited)
					{
						ImGui::PushID(key.c_str());
						changed |= EditJson(key.c_str(), member, depth + 1);
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
				if (changed) value = JsonValue(std::move(edited));
				return changed;
			}
			ImGui::TextDisabled("%s: (empty)", label);
			return false;
		}

		// One field, by its registered kind. True if the user changed it, with the new value in `value`.
		bool EditField(const FieldInfo& field, JsonValue& value, const Registry& registry,
		               const std::vector<Entity>& all)
		{
			const char* label = field.name.c_str();
			bool changed = false;
			if (field.hints.readOnly) ImGui::BeginDisabled();
			switch (field.kind)
			{
			case FieldKind::Bool:
			case FieldKind::String:
				changed = EditJson(label, value, 0);
				break;
			case FieldKind::Int:
			case FieldKind::Float:
			{
				const bool isInt = field.kind == FieldKind::Int;
				if (field.hints.min && field.hints.max)
				{
					if (isInt)
					{
						auto number = value.AsInt().value_or(0);
						auto low = static_cast<std::int64_t>(*field.hints.min);
						auto high = static_cast<std::int64_t>(*field.hints.max);
						if (ImGui::SliderScalar(label, ImGuiDataType_S64, &number, &low, &high))
						{
							value = JsonValue(number);
							changed = true;
						}
					}
					else
					{
						double number = value.AsDouble().value_or(0.0);
						double low = *field.hints.min;
						double high = *field.hints.max;
						if (ImGui::SliderScalar(label, ImGuiDataType_Double, &number, &low, &high))
						{
							value = JsonValue(number);
							changed = true;
						}
					}
				}
				else if (isInt)
				{
					auto number = value.AsInt().value_or(0);
					if (ImGui::DragScalar(label, ImGuiDataType_S64, &number, 1.0F))
					{
						value = JsonValue(number);
						changed = true;
					}
				}
				else
				{
					double number = value.AsDouble().value_or(0.0);
					if (ImGui::DragScalar(label, ImGuiDataType_Double, &number, 0.05F))
					{
						value = JsonValue(number);
						changed = true;
					}
				}
				break;
			}
			case FieldKind::Enum:
			{
				const std::string current(value.AsString().value_or(""));
				if (ImGui::BeginCombo(label, current.c_str()))
				{
					for (const std::string& name : field.enumNames)
					{
						if (ImGui::Selectable(name.c_str(), name == current))
						{
							value = JsonValue(name);
							changed = true;
						}
					}
					ImGui::EndCombo();
				}
				break;
			}
			case FieldKind::Entity:
			{
				std::optional<std::int64_t> id = value.AsInt();
				if (EntityCombo(label, registry, all, id))
				{
					value = id ? JsonValue(*id) : JsonValue();
					changed = true;
				}
				break;
			}
			case FieldKind::EntityList:
			{
				JsonArray items = value.AsArray() != nullptr ? *value.AsArray() : JsonArray{};
				if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (std::size_t i = 0; i < items.size(); ++i)
					{
						ImGui::PushID(static_cast<int>(i));
						std::optional<std::int64_t> id = items[i].AsInt();
						if (EntityCombo("##entity", registry, all, id) && id)
						{
							items[i] = JsonValue(*id);
							changed = true;
						}
						ImGui::SameLine();
						if (ImGui::SmallButton("x"))
						{
							items.erase(items.begin() + static_cast<std::ptrdiff_t>(i));
							changed = true;
							ImGui::PopID();
							break;
						}
						ImGui::PopID();
					}
					std::optional<std::int64_t> added;
					if (EntityCombo("add", registry, all, added) && added)
					{
						items.emplace_back(*added);
						changed = true;
					}
					ImGui::TreePop();
				}
				if (changed) value = JsonValue(std::move(items));
				break;
			}
			case FieldKind::Custom:
				changed = EditJson(label, value, 0);
				break;
			}
			if (field.hints.readOnly) ImGui::EndDisabled();
			if (!field.hints.tooltip.empty() && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", field.hints.tooltip.c_str());
			return changed;
		}

		// Draws one component's fields and writes back the ones the user changed.
		void DrawComponent(Registry& registry, const TypeInfo& type, Entity entity, PanelState& state,
		                   const std::vector<Entity>& all, const LiveIds& ids)
		{
			void* component = type.Get(registry, entity);
			if (component == nullptr) return;
			for (const FieldInfo& field : type.Fields())
			{
				JsonValue value;
				std::string error;
				if (!field.save(component, ids, value, error))
				{
					ImGui::TextDisabled("%s: %s", field.name.c_str(), error.c_str());
					continue;
				}
				if (!EditField(field, value, registry, all)) continue;

				// A new parent must not make a loop, which would hang anything that walks up the hierarchy.
				if (type.Type() == std::type_index(typeid(ecs::Parent)))
				{
					const auto parentId = value.AsInt();
					const auto parentEntity = parentId ? ids.EntityOf(*parentId) : std::nullopt;
					if (parentEntity && WouldMakeLoop(registry, entity, *parentEntity))
					{
						state.status = "can't parent " + LabelOf(registry, entity) + " to itself or something under it";
						continue;
					}
				}
				if (!field.load(component, value, ids, error))
				{
					state.status = type.Name() + "." + field.name + ": " + error;
				}
				else
				{
					state.status.clear();
				}
			}
		}

		void DrawInspector(ecs::World& world, PanelState& state)
		{
			Registry& registry = world.registry;
			const TypeRegistry* types = world.resources.TryGet<TypeRegistry>();
			if (types == nullptr)
			{
				ImGui::TextDisabled("No TypeRegistry resource, nothing to show.");
				return;
			}
			if (!registry.IsAlive(state.selected)) state.selected = Entity::Null;
			if (state.selected.IsNull())
			{
				ImGui::TextDisabled("Select an entity in the Hierarchy.");
				return;
			}
			const Entity entity = state.selected;
			ImGui::Text("%s  (index %u)", LabelOf(registry, entity).c_str(), static_cast<unsigned>(entity.id));
			if (!state.status.empty()) ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "%s", state.status.c_str());
			ImGui::Separator();

			const std::vector<Entity> all = registry.AllEntities();
			const LiveIds ids(registry); // once per frame, not per component
			const TypeInfo* toRemove = nullptr;
			for (std::size_t i = 0; i < types->Count(); ++i)
			{
				const TypeInfo& type = types->At(i);
				if (type.IsRuntime() || !type.Has(registry, entity)) continue;
				ImGui::PushID(static_cast<int>(i));
				bool keep = true;
				if (ImGui::CollapsingHeader(type.Name().c_str(), &keep, ImGuiTreeNodeFlags_DefaultOpen))
				{
					DrawComponent(registry, type, entity, state, all, ids);
				}
				if (!keep) toRemove = &type;
				ImGui::PopID();
			}
			if (toRemove != nullptr) toRemove->Remove(registry, entity);

			ImGui::Spacing();
			if (ImGui::BeginCombo("Add component", "choose..."))
			{
				for (std::size_t i = 0; i < types->Count(); ++i)
				{
					const TypeInfo& type = types->At(i);
					if (type.IsRuntime() || type.Has(registry, entity)) continue;
					if (ImGui::Selectable(type.Name().c_str())) type.AddDefault(registry, entity);
				}
				ImGui::EndCombo();
			}
		}
	} // namespace

	std::shared_ptr<PanelState> AddEntityPanels(Editor& editor, std::function<ecs::World&()> world)
	{
		const auto state = std::make_shared<PanelState>();
		editor.AddPanel("Hierarchy", [world, state]
		{
			DrawHierarchy(world(), *state);
		});
		editor.AddPanel("Inspector", [world, state]
		{
			DrawInspector(world(), *state);
		});
		editor.SetPanelOpen("Hierarchy", true);
		editor.SetPanelOpen("Inspector", true);
		return state;
	}

	std::shared_ptr<PanelState> AddEntityPanels(Editor& editor, ecs::World& world)
	{
		return AddEntityPanels(editor, [&world]() -> ecs::World&
		{
			return world;
		});
	}
} // namespace engine::editor
