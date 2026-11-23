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
			if (state.IsSelected(entity)) flags |= ImGuiTreeNodeFlags_Selected;
			if (!state.filter.empty()) flags |= ImGuiTreeNodeFlags_DefaultOpen;

			ImGui::PushID(static_cast<int>(entity.id));
			const bool open = ImGui::TreeNodeEx(LabelOf(registry, entity).c_str(), flags);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
			{
				// Ctrl or Shift adds to the selection (or takes the entity out of it); a plain click selects just it.
				if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift)
					state.Toggle(entity);
				else
					state.Select(entity);
			}

			if (ImGui::BeginPopupContextItem())
			{
				if (!state.IsSelected(entity)) state.Select(entity); // right-clicking part of a selection keeps it
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
				state.Select(CreateEntity(registry, "New Entity", pending.target));
				break;
			case Pending::Kind::Duplicate:
			{
				if (types == nullptr) break;
				if (!state.IsSelected(pending.target)) state.Select(pending.target);
				DuplicateSelection(registry, *types, state);
				break;
			}
			case Pending::Kind::Delete:
				if (!state.IsSelected(pending.target)) state.Select(pending.target);
				DeleteSelection(registry, state);
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

			if (ImGui::Button("New entity")) state.Select(CreateEntity(registry, "New Entity"));
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

		// A text field picked from a list. A value that is not in the list is shown as it is, marked, and stays
		// until another is picked.
		bool EditChoice(const char* label, const std::vector<std::string>& choices, JsonValue& value)
		{
			const std::string current(value.AsString().value_or(""));
			const bool known = std::find(choices.begin(), choices.end(), current) != choices.end();
			const std::string shown = known || current.empty() ? current : current + " (not in the list)";
			bool changed = false;
			if (ImGui::BeginCombo(label, shown.c_str()))
			{
				for (const std::string& choice : choices)
				{
					if (!ImGui::Selectable(choice.c_str(), choice == current)) continue;
					value = JsonValue(choice);
					changed = true;
				}
				ImGui::EndCombo();
			}
			return changed;
		}

		std::optional<graphics::PixelRect> RectOf(const JsonValue& value)
		{
			const core::JsonObject* members = value.AsObject();
			if (members == nullptr) return std::nullopt;
			graphics::PixelRect rect;
			for (const auto& [key, member] : *members)
			{
				const auto number = member.AsInt();
				if (!number) continue;
				const int whole = static_cast<int>(*number);
				if (key == "x") rect.x = whole;
				if (key == "y") rect.y = whole;
				if (key == "width") rect.width = whole;
				if (key == "height") rect.height = whole;
			}
			return rect;
		}

		JsonValue RectJson(const graphics::PixelRect& rect)
		{
			core::JsonObject members;
			members.emplace_back("x", JsonValue(static_cast<std::int64_t>(rect.x)));
			members.emplace_back("y", JsonValue(static_cast<std::int64_t>(rect.y)));
			members.emplace_back("width", JsonValue(static_cast<std::int64_t>(rect.width)));
			members.emplace_back("height", JsonValue(static_cast<std::int64_t>(rect.height)));
			return JsonValue(std::move(members));
		}

		// A picker of sprite names for a rectangle in the atlas: the sprites of the catalog that the game can draw.
		// Picking one writes its rectangle; the combo shows the name whose rectangle the field holds, if any.
		bool EditSpriteSource(const AssetCatalog& assets,
		                      const std::function<std::optional<graphics::PixelRect>(const std::string&)>& rectOf,
		                      JsonValue& value)
		{
			const std::optional<graphics::PixelRect> current = RectOf(value);
			std::string shown = "(custom rectangle)";
			std::vector<std::pair<std::string, graphics::PixelRect>> options;
			for (const std::string& name : assets.Names(ecs::AssetKind::Sprite))
			{
				const std::optional<graphics::PixelRect> rect = rectOf(name);
				if (!rect) continue;
				options.emplace_back(name, *rect);
				if (current && *current == *rect && shown == "(custom rectangle)") shown = name;
			}
			bool changed = false;
			if (ImGui::BeginCombo("picture", shown.c_str()))
			{
				for (const auto& [name, rect] : options)
				{
					if (!ImGui::Selectable(name.c_str(), name == shown)) continue;
					value = RectJson(rect);
					changed = true;
				}
				ImGui::EndCombo();
			}
			return changed;
		}

		// One field, by its registered kind. True if the user changed it, with the new value in `value`.
		bool EditField(const FieldInfo& field, JsonValue& value, const Registry& registry,
		               const std::vector<Entity>& all, const PanelState& state)
		{
			const AssetCatalog* assets = state.assets.get();
			const char* label = field.name.c_str();
			bool changed = false;
			if (field.hints.readOnly) ImGui::BeginDisabled();
			switch (field.kind)
			{
			case FieldKind::String:
				if (field.hints.assetKind != ecs::AssetKind::None && assets != nullptr &&
				    !assets->Names(field.hints.assetKind).empty())
				{
					// The files found under the assets folder (the Assets panel lists the same ones), cut down to the
					// names the game accepts when the field says which those are.
					std::vector<std::string> names = assets->Names(field.hints.assetKind);
					const std::vector<std::string>& usable = field.hints.choices;
					if (!usable.empty())
						std::erase_if(names, [&usable](const std::string& name)
						{
							return std::find(usable.begin(), usable.end(), name) == usable.end();
						});
					changed = EditChoice(label, names.empty() ? usable : names, value);
					break;
				}
				if (!field.hints.choices.empty())
				{
					changed = EditChoice(label, field.hints.choices, value);
					break;
				}
				changed = EditJson(label, value, 0);
				break;
			case FieldKind::Bool:
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
				if (field.hints.assetKind == ecs::AssetKind::Sprite && assets != nullptr && state.spriteRect)
					changed = EditSpriteSource(*assets, state.spriteRect, value);
				changed |= EditJson(label, value, 0);
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
			// A filter that matches the component's name shows all its fields; otherwise only the matching ones.
			const bool wholeType = ContainsNoCase(type.Name(), state.fieldFilter);
			for (const FieldInfo& field : type.Fields())
			{
				if (!wholeType && !ContainsNoCase(field.name, state.fieldFilter)) continue;
				JsonValue value;
				std::string error;
				if (!field.save(component, ids, value, error))
				{
					ImGui::TextDisabled("%s: %s", field.name.c_str(), error.c_str());
					continue;
				}
				if (!EditField(field, value, registry, all, state)) continue;

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
			state.Prune(registry);
			if (state.selected.IsNull())
			{
				ImGui::TextDisabled("Select an entity in the Hierarchy.");
				return;
			}
			const Entity entity = state.selected;
			ImGui::Text("%s  (index %u)", LabelOf(registry, entity).c_str(), static_cast<unsigned>(entity.id));
			if (!state.extra.empty())
				ImGui::TextDisabled("%u more selected; changes here apply to this entity only",
				                    static_cast<unsigned>(state.extra.size()));
			if (!state.status.empty()) ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "%s", state.status.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##fieldfilter", "search components and fields", &state.fieldFilter);
			ImGui::Separator();

			const std::vector<Entity> all = registry.AllEntities();
			const LiveIds ids(registry); // once per frame, not per component
			const TypeInfo* toRemove = nullptr;
			for (std::size_t i = 0; i < types->Count(); ++i)
			{
				const TypeInfo& type = types->At(i);
				if (type.IsRuntime() || !type.Has(registry, entity)) continue;
				if (!state.fieldFilter.empty() && !ContainsNoCase(type.Name(), state.fieldFilter))
				{
					const auto& fields = type.Fields();
					const bool any = std::any_of(fields.begin(), fields.end(), [&](const FieldInfo& field)
					{
						return ContainsNoCase(field.name, state.fieldFilter);
					});
					if (!any) continue;
				}
				ImGui::PushID(static_cast<int>(i));
				bool keep = true;
				const bool open = ImGui::CollapsingHeader(type.Name().c_str(), &keep, ImGuiTreeNodeFlags_DefaultOpen);
				if (ImGui::BeginPopupContextItem("component menu"))
				{
					if (ImGui::MenuItem("Copy values"))
					{
						std::string error;
						const std::string text = CopyComponent(registry, type, entity, error);
						if (text.empty())
							state.status = error;
						else
							ImGui::SetClipboardText(text.c_str());
					}
					if (ImGui::MenuItem("Paste values"))
					{
						std::string error;
						const char* clip = ImGui::GetClipboardText();
						if (!PasteComponent(registry, *types, entity, clip != nullptr ? clip : "", error, &type))
							state.status = error;
						else
							state.status.clear();
					}
					ImGui::EndPopup();
				}
				if (open) DrawComponent(registry, type, entity, state, all, ids);
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
		editor.SetPanelDock("Hierarchy", Editor::DockArea::Left);
		editor.SetPanelDock("Inspector", Editor::DockArea::Right);
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

	std::vector<ecs::Entity> PanelState::All() const
	{
		std::vector<ecs::Entity> all;
		if (!selected.IsNull()) all.push_back(selected);
		for (const ecs::Entity entity : extra)
			if (entity != selected) all.push_back(entity);
		return all;
	}

	bool PanelState::IsSelected(ecs::Entity entity) const
	{
		if (entity.IsNull()) return false;
		return entity == selected || std::find(extra.begin(), extra.end(), entity) != extra.end();
	}

	void PanelState::Select(ecs::Entity entity)
	{
		selected = entity;
		extra.clear();
	}

	void PanelState::Toggle(ecs::Entity entity)
	{
		if (entity.IsNull()) return;
		std::vector<ecs::Entity> all = All();
		const auto found = std::find(all.begin(), all.end(), entity);
		if (found != all.end())
			all.erase(found);
		else
			all.insert(all.begin(), entity);
		selected = all.empty() ? ecs::Entity::Null : all.front();
		extra.assign(all.empty() ? all.end() : all.begin() + 1, all.end());
	}

	void PanelState::Prune(const ecs::Registry& registry)
	{
		std::vector<ecs::Entity> all = All();
		all.erase(std::remove_if(all.begin(), all.end(),
		                         [&registry](ecs::Entity entity)
		{
			return !registry.IsAlive(entity);
		}),
		          all.end());
		selected = all.empty() ? ecs::Entity::Null : all.front();
		extra.assign(all.empty() ? all.end() : all.begin() + 1, all.end());
	}

	namespace
	{
		// True if some other entity of the selection is an ancestor of entity.
		bool HasSelectedAncestor(const ecs::Registry& registry, const PanelState& state, ecs::Entity entity)
		{
			ecs::Entity at = entity;
			for (int depth = 0; depth < 256; ++depth)
			{
				const auto* parent = registry.TryGetComponent<ecs::Parent>(at);
				if (parent == nullptr || parent->entity.IsNull()) return false;
				if (state.IsSelected(parent->entity)) return true;
				at = parent->entity;
			}
			return false;
		}
	} // namespace

	std::vector<ecs::Entity> SelectionRoots(const ecs::Registry& registry, const PanelState& state)
	{
		std::vector<ecs::Entity> roots;
		for (const ecs::Entity entity : state.All())
			if (registry.IsAlive(entity) && !HasSelectedAncestor(registry, state, entity)) roots.push_back(entity);
		return roots;
	}

	void DeleteSelection(ecs::Registry& registry, PanelState& state)
	{
		for (const ecs::Entity entity : state.All())
			if (registry.IsAlive(entity)) DestroyTree(registry, entity);
		state.Select(ecs::Entity::Null);
	}

	void DuplicateSelection(ecs::Registry& registry, const ecs::TypeRegistry& types, PanelState& state)
	{
		std::vector<ecs::Entity> copies;
		for (const ecs::Entity entity : SelectionRoots(registry, state))
		{
			ecs::Issues issues;
			const ecs::Entity copy = DuplicateTree(registry, types, entity, issues);
			if (!copy.IsNull())
				copies.push_back(copy);
			else if (!issues.errors.empty())
				state.status = "can't duplicate: " + issues.errors.front().path + ": " + issues.errors.front().message;
		}
		if (copies.empty()) return;
		state.Select(copies.front());
		state.extra.assign(copies.begin() + 1, copies.end());
	}
} // namespace engine::editor
