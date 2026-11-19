#pragma once
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

#include "core/assert.hpp"
#include "core/json.hpp"
#include "ecs/component.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"

// The type registry: component types described at run time (a name, and a list of fields) so
// code that does not know the types can still list them, save them as JSON and load them back.
// It is what scenes (ecs/scene.hpp) and an editor are built on. Opt-in: a game that never
// registers a type pays nothing, and Registry and ComponentPool know nothing about it.
//
// Registration is explicit, one block per component, written next to the component:
//
//     types.Register<Shape>("Shape")
//         .EnumField("kind", &Shape::kind, {{"Rectangle", ShapeKind::Rectangle}, {"Circle", ShapeKind::Circle}})
//         .Field("width", &Shape::width)
//         .Field("color", &Shape::color)
//         .Field("layer", &Shape::layer, {.min = 0, .max = 255});
//     types.RegisterTag<ScreenSpace>("ScreenSpace");
//
// A field type must have a FieldCodec (below); one that does not is a compile error, so a raw
// pointer, a std::function or a container nobody described can't be saved by accident. Leave
// such members out of the registration, or register the component with Runtime().
namespace engine::ecs
{
	// Maps entities to the integer ids a saved file uses, and back. A saved file never holds an
	// in-memory Entity, whose index and generation change from run to run.
	class IEntityIds
	{
	public:
		virtual ~IEntityIds() = default;
		// The id of entity in the file being written, or nothing if it is not part of it.
		[[nodiscard]] virtual std::optional<std::int64_t> IdOf(Entity entity) const = 0;
		// The entity that has id in the file being read, or nothing if there is none.
		[[nodiscard]] virtual std::optional<Entity> EntityOf(std::int64_t id) const = 0;
	};

	// Problems found while saving or loading, each with a JSON path such as
	// "entities[3].components.Follow.target". Errors stop the component from being added;
	// warnings (an unknown field) do not.
	struct Issue
	{
		std::string path;
		std::string message;
	};

	struct Issues
	{
		std::vector<Issue> errors;
		std::vector<Issue> warnings;

		[[nodiscard]] bool Ok() const noexcept { return errors.empty(); }
	};

	// What an editor shows for a field. min and max are hints for a slider; loading does not clamp.
	struct FieldHints
	{
		std::optional<double> min = std::nullopt;
		std::optional<double> max = std::nullopt;
		std::string tooltip = {};
		bool readOnly = false;
	};

	enum class FieldKind : std::uint8_t
	{
		Bool,
		Int,
		Float,
		String,
		Entity,
		EntityList,
		Enum,
		Custom // an object or array with its own codec, such as a Color
	};

	// How a field type is written to JSON and read back. Specialise it for a type of your own:
	//
	//     template <> struct FieldCodec<Color>
	//     {
	//         static constexpr FieldKind kind = FieldKind::Custom;
	//         static bool Save(const Color&, const IEntityIds&, core::JsonValue& out, std::string& error);
	//         static bool Load(Color&, const core::JsonValue&, const IEntityIds&, std::string& error);
	//     };
	//
	// Save and Load return false with a message in error; the registry adds the field's path.
	template <typename T> struct FieldCodec;

	namespace detail
	{
		// A float as the double that prints shortest: 0.1f is written as 0.1, not 0.10000000149011612.
		[[nodiscard]] double FloatToJsonDouble(float value);
		inline std::string Expected(std::string_view what, const core::JsonValue& got)
		{
			const char* kind = got.IsNull()     ? "null"
			                   : got.IsBool()   ? "a bool"
			                   : got.IsNumber() ? "a number"
			                   : got.IsString() ? "a string"
			                   : got.IsArray()  ? "an array"
			                                    : "an object";
			return "expected " + std::string(what) + ", got " + kind;
		}
	} // namespace detail

	template <> struct FieldCodec<bool>
	{
		static constexpr FieldKind kind = FieldKind::Bool;
		static bool Save(const bool& value, const IEntityIds&, core::JsonValue& out, std::string&)
		{
			out = core::JsonValue(value);
			return true;
		}
		static bool Load(bool& value, const core::JsonValue& in, const IEntityIds&, std::string& error)
		{
			const auto parsed = in.AsBool();
			if (!parsed)
			{
				error = detail::Expected("a bool", in);
				return false;
			}
			value = *parsed;
			return true;
		}
	};

	template <typename T>
	    requires(std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, char> && !std::same_as<T, wchar_t> &&
	             !std::same_as<T, char8_t> && !std::same_as<T, char16_t> && !std::same_as<T, char32_t>)
	struct FieldCodec<T>
	{
		static constexpr FieldKind kind = FieldKind::Int;
		static bool Save(const T& value, const IEntityIds&, core::JsonValue& out, std::string&)
		{
			out = core::JsonValue(value);
			return true;
		}
		static bool Load(T& value, const core::JsonValue& in, const IEntityIds&, std::string& error)
		{
			const auto parsed = in.AsInt();
			if (!parsed)
			{
				error = detail::Expected("an integer", in);
				return false;
			}
			if (!std::in_range<T>(*parsed))
			{
				error = "the integer " + std::to_string(*parsed) + " does not fit";
				return false;
			}
			value = static_cast<T>(*parsed);
			return true;
		}
	};

	template <std::floating_point T> struct FieldCodec<T>
	{
		static constexpr FieldKind kind = FieldKind::Float;
		static bool Save(const T& value, const IEntityIds&, core::JsonValue& out, std::string& error)
		{
			if (!std::isfinite(value))
			{
				error = "NaN and infinity have no JSON form";
				return false;
			}
			if constexpr (std::same_as<T, float>)
			{
				out = core::JsonValue(detail::FloatToJsonDouble(value));
			}
			else
			{
				out = core::JsonValue(static_cast<double>(value));
			}
			return true;
		}
		static bool Load(T& value, const core::JsonValue& in, const IEntityIds&, std::string& error)
		{
			const auto parsed = in.AsDouble();
			if (!parsed)
			{
				error = detail::Expected("a number", in);
				return false;
			}
			if (std::fabs(*parsed) > static_cast<double>(std::numeric_limits<T>::max()))
			{
				error = "the number is too large";
				return false;
			}
			value = static_cast<T>(*parsed);
			return true;
		}
	};

	template <> struct FieldCodec<std::string>
	{
		static constexpr FieldKind kind = FieldKind::String;
		static bool Save(const std::string& value, const IEntityIds&, core::JsonValue& out, std::string&)
		{
			out = core::JsonValue(value);
			return true;
		}
		static bool Load(std::string& value, const core::JsonValue& in, const IEntityIds&, std::string& error)
		{
			const auto parsed = in.AsString();
			if (!parsed)
			{
				error = detail::Expected("a string", in);
				return false;
			}
			value = std::string(*parsed);
			return true;
		}
	};

	// An entity is written as its id in the file (see IEntityIds), and the null entity as null.
	template <> struct FieldCodec<Entity>
	{
		static constexpr FieldKind kind = FieldKind::Entity;
		static bool Save(const Entity& value, const IEntityIds& ids, core::JsonValue& out, std::string& error);
		static bool Load(Entity& value, const core::JsonValue& in, const IEntityIds& ids, std::string& error);
	};

	template <> struct FieldCodec<std::vector<Entity>>
	{
		static constexpr FieldKind kind = FieldKind::EntityList;
		static bool Save(const std::vector<Entity>& value, const IEntityIds& ids, core::JsonValue& out,
		                 std::string& error);
		static bool Load(std::vector<Entity>& value, const core::JsonValue& in, const IEntityIds& ids,
		                 std::string& error);
	};

	// A field as the registry stores it: what an inspector needs to draw it, and the two functions
	// that move it to and from JSON. The functions take a pointer to the component that owns it.
	struct FieldInfo
	{
		std::string name;
		FieldKind kind = FieldKind::Custom;
		FieldHints hints;
		std::vector<std::string> enumNames; // the choices of an Enum field, in the order registered
		std::function<bool(const void* component, const IEntityIds&, core::JsonValue& out, std::string& error)> save;
		std::function<bool(void* component, const core::JsonValue& in, const IEntityIds&, std::string& error)> load;
	};

	// One registered component type.
	class TypeInfo
	{
		friend class TypeRegistry;
		template <Component T> friend class TypeBuilder;

		std::string name;
		std::type_index type = std::type_index(typeid(void));
		bool runtime = false;
		std::vector<FieldInfo> fields;

		std::function<bool(const Registry&, Entity)> has;
		std::function<void(Registry&, Entity)> addDefault;
		std::function<void(Registry&, Entity)> remove;
		std::function<void*(Registry&, Entity)> get;
		// Builds a default T, lets fill change it, and adds it through Registry::Add (so OnAdd
		// hooks run) unless fill returns false.
		std::function<bool(Registry&, Entity, const std::function<bool(void*)>& fill)> addFilled;

	public:
		[[nodiscard]] const std::string& Name() const noexcept { return name; }
		[[nodiscard]] std::type_index Type() const noexcept { return type; }
		// A component the engine's own systems keep up to date (previous positions, caches): it is
		// neither saved nor shown for editing.
		[[nodiscard]] bool IsRuntime() const noexcept { return runtime; }
		[[nodiscard]] const std::vector<FieldInfo>& Fields() const noexcept { return fields; }

		[[nodiscard]] bool Has(const Registry& registry, Entity entity) const { return has(registry, entity); }
		// Adds a default-constructed component. The entity must be alive.
		void AddDefault(Registry& registry, Entity entity) const { addDefault(registry, entity); }
		void Remove(Registry& registry, Entity entity) const { remove(registry, entity); }
		// The component, or nullptr if the entity does not have it. For a field's save and load.
		[[nodiscard]] void* Get(Registry& registry, Entity entity) const { return get(registry, entity); }
		[[nodiscard]] const void* Get(const Registry& registry, Entity entity) const
		{
			return get(const_cast<Registry&>(registry), entity);
		}

		// The entity's component as a JSON object keyed by field name. The entity must have it.
		// False, with errors in issues under path, if a field can't be saved.
		bool Save(const Registry& registry, Entity entity, const IEntityIds& ids, core::JsonValue& out, Issues& issues,
		          const std::string& path) const;

		// Adds the component to the entity from a JSON object. A field that is missing keeps its
		// default; a field that is not registered is a warning. If any field is wrong the component
		// is not added, every problem is reported, and the result is false. A component the entity
		// already has is replaced by the loaded one.
		bool Load(Registry& registry, Entity entity, const core::JsonValue& in, const IEntityIds& ids, Issues& issues,
		          const std::string& path) const;
	};

	// Chained registration for one component type, returned by TypeRegistry::Register.
	template <Component T> class TypeBuilder
	{
		TypeInfo& info;

		void AddField(FieldInfo field)
		{
			for (const FieldInfo& existing : info.fields)
			{
				ENGINE_VERIFY(existing.name != field.name, "a field name registered twice for one component");
			}
			info.fields.push_back(std::move(field));
		}

	public:
		explicit TypeBuilder(TypeInfo& typeInfo) : info(typeInfo) {}

		// A field whose type has a FieldCodec.
		template <typename F>
		    requires requires { FieldCodec<F>::kind; }
		TypeBuilder& Field(std::string_view name, F T::*member, FieldHints hints = {})
		{
			FieldInfo field;
			field.name = std::string(name);
			field.kind = FieldCodec<F>::kind;
			field.hints = std::move(hints);
			field.save =
			    [member](const void* component, const IEntityIds& ids, core::JsonValue& out, std::string& error)
			{
				return FieldCodec<F>::Save(static_cast<const T*>(component)->*member, ids, out, error);
			};
			field.load = [member](void* component, const core::JsonValue& in, const IEntityIds& ids, std::string& error)
			{
				return FieldCodec<F>::Load(static_cast<T*>(component)->*member, in, ids, error);
			};
			AddField(std::move(field));
			return *this;
		}

		// An enum field, written as the name given for the value. Every value the field can hold
		// must have a name, or saving it is an error.
		template <typename E>
		    requires std::is_enum_v<E>
		TypeBuilder& EnumField(std::string_view name, E T::*member,
		                       std::initializer_list<std::pair<std::string_view, E>> names, FieldHints hints = {})
		{
			ENGINE_VERIFY(names.size() > 0, "an enum field needs at least one name");
			std::vector<std::pair<std::string, E>> table;
			FieldInfo field;
			for (const auto& entry : names)
			{
				table.emplace_back(std::string(entry.first), entry.second);
				field.enumNames.emplace_back(entry.first);
			}
			field.name = std::string(name);
			field.kind = FieldKind::Enum;
			field.hints = std::move(hints);
			field.save =
			    [member, table](const void* component, const IEntityIds&, core::JsonValue& out, std::string& error)
			{
				const E value = static_cast<const T*>(component)->*member;
				for (const auto& entry : table)
				{
					if (entry.second == value)
					{
						out = core::JsonValue(entry.first);
						return true;
					}
				}
				error = "an enum value with no registered name";
				return false;
			};
			field.load =
			    [member, table](void* component, const core::JsonValue& in, const IEntityIds&, std::string& error)
			{
				const auto text = in.AsString();
				if (!text)
				{
					error = detail::Expected("a string", in);
					return false;
				}
				for (const auto& entry : table)
				{
					if (entry.first == *text)
					{
						static_cast<T*>(component)->*member = entry.second;
						return true;
					}
				}
				error = "unknown value \"" + std::string(*text) + "\"";
				return false;
			};
			AddField(std::move(field));
			return *this;
		}

		// Marks the component as one the engine's systems own: not saved, not shown for editing.
		TypeBuilder& Runtime()
		{
			info.runtime = true;
			return *this;
		}
	};

	class TypeRegistry
	{
		// unique_ptr so a TypeInfo keeps its address as more types are registered.
		std::vector<std::unique_ptr<TypeInfo>> types;

		TypeInfo& AddType(std::string_view name, std::type_index type);

	public:
		// Registers component type T under name, and returns a builder to add its fields. The
		// name is what a saved file calls the component, so renaming it breaks old files. A name
		// or a type registered twice is a checked error. T must be default-constructible.
		template <Component T>
		    requires std::default_initializable<T>
		TypeBuilder<T> Register(std::string_view name)
		{
			TypeInfo& info = AddType(name, std::type_index(typeid(T)));
			info.has = [](const Registry& registry, Entity entity)
			{
				return registry.HasComponent<T>(entity);
			};
			info.addDefault = [](Registry& registry, Entity entity)
			{
				registry.Add<T>(entity);
			};
			info.remove = [](Registry& registry, Entity entity)
			{
				registry.Remove<T>(entity);
			};
			info.get = [](Registry& registry, Entity entity) -> void*
			{
				return registry.TryGetComponent<T>(entity);
			};
			info.addFilled = [](Registry& registry, Entity entity, const std::function<bool(void*)>& fill)
			{
				T value{};
				if (!fill(&value)) return false;
				if (T* existing = registry.TryGetComponent<T>(entity))
				{
					*existing = std::move(value); // replaced in place; no OnAdd hook, it was already there
					return true;
				}
				registry.Add<T>(entity, std::move(value));
				return true;
			};
			return TypeBuilder<T>(info);
		}

		// A component with no fields, such as a marker: it is saved as {}.
		template <Component T>
		    requires std::default_initializable<T>
		void RegisterTag(std::string_view name)
		{
			Register<T>(name);
		}

		// The type registered under name, or nullptr.
		[[nodiscard]] const TypeInfo* Find(std::string_view name) const noexcept;
		[[nodiscard]] const TypeInfo* Find(std::type_index type) const noexcept;
		template <Component T> [[nodiscard]] const TypeInfo* Find() const noexcept
		{
			return Find(std::type_index(typeid(T)));
		}

		// Every type, in the order registered (so saved files list components in a stable order).
		[[nodiscard]] std::size_t Count() const noexcept { return types.size(); }
		[[nodiscard]] const TypeInfo& At(std::size_t index) const;
	};
} // namespace engine::ecs
