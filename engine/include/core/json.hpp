#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/text.hpp"

// JSON for data files and saves: a value tree, a parser that reports line and column, and a
// deterministic writer. Pure apart from LoadJson and WriteJson: no SDL, no exceptions.
//
// Choices that matter for data files:
//  - Objects keep the order of their keys, so written files diff cleanly. A key given twice is a
//    parse error.
//  - A number with no fraction or exponent that fits is stored as int64_t, never as a double, so
//    seeds and IDs round-trip exactly. Every other number is a double.
//  - Doubles are written in the shortest form that reads back as the same value. NaN and
//    infinity have no JSON form and are written as null.
namespace engine::core
{
	class JsonValue;
	using JsonArray = std::vector<JsonValue>;
	using JsonObject = std::vector<std::pair<std::string, JsonValue>>; // keeps key order

	class JsonValue
	{
		using Storage = std::variant<std::monostate, bool, std::int64_t, double, std::string, JsonArray, JsonObject>;
		Storage storage;

	public:
		JsonValue() = default; // null
		JsonValue(std::nullptr_t) {}
		JsonValue(bool value) : storage(value) {}
		template <std::integral T>
		    requires(!std::same_as<T, bool>)
		JsonValue(T value) : storage(static_cast<std::int64_t>(value))
		{
		}
		JsonValue(double value) : storage(value) {}
		JsonValue(std::string value) : storage(std::move(value)) {}
		JsonValue(std::string_view value) : storage(std::string(value)) {}
		JsonValue(const char* value) : storage(std::string(value)) {}
		JsonValue(JsonArray value) : storage(std::move(value)) {}
		JsonValue(JsonObject value) : storage(std::move(value)) {}

		[[nodiscard]] static JsonValue Array() { return JsonValue(JsonArray{}); }
		[[nodiscard]] static JsonValue Object() { return JsonValue(JsonObject{}); }

		[[nodiscard]] bool IsNull() const noexcept { return std::holds_alternative<std::monostate>(storage); }
		[[nodiscard]] bool IsBool() const noexcept { return std::holds_alternative<bool>(storage); }
		[[nodiscard]] bool IsInt() const noexcept { return std::holds_alternative<std::int64_t>(storage); }
		[[nodiscard]] bool IsDouble() const noexcept { return std::holds_alternative<double>(storage); }
		[[nodiscard]] bool IsNumber() const noexcept { return IsInt() || IsDouble(); }
		[[nodiscard]] bool IsString() const noexcept { return std::holds_alternative<std::string>(storage); }
		[[nodiscard]] bool IsArray() const noexcept { return std::holds_alternative<JsonArray>(storage); }
		[[nodiscard]] bool IsObject() const noexcept { return std::holds_alternative<JsonObject>(storage); }

		// Nothing if the value is not of that kind. AsDouble also accepts an integer.
		[[nodiscard]] std::optional<bool> AsBool() const noexcept;
		[[nodiscard]] std::optional<std::int64_t> AsInt() const noexcept;
		[[nodiscard]] std::optional<double> AsDouble() const noexcept;
		[[nodiscard]] std::optional<std::string_view> AsString() const noexcept;
		[[nodiscard]] const JsonArray* AsArray() const noexcept { return std::get_if<JsonArray>(&storage); }
		[[nodiscard]] const JsonObject* AsObject() const noexcept { return std::get_if<JsonObject>(&storage); }

		// The member called key if this is an object that has one, else nullptr.
		[[nodiscard]] const JsonValue* Find(std::string_view key) const noexcept;
		[[nodiscard]] JsonValue* Find(std::string_view key) noexcept;

		// Array element i. This must be an array and i in range.
		[[nodiscard]] const JsonValue& operator[](std::size_t index) const;
		[[nodiscard]] JsonValue& operator[](std::size_t index);

		// The number of elements of an array or members of an object, else 0.
		[[nodiscard]] std::size_t Size() const noexcept;

		// Building. This must be an object (Set) or an array (Append). Set replaces the value of a
		// key that is already there, in place, and otherwise adds the key at the end.
		JsonValue& Set(std::string_view key, JsonValue value);
		JsonValue& Append(JsonValue value);

		[[nodiscard]] friend bool operator==(const JsonValue&, const JsonValue&) = default;
	};

	// Either the value, or the first error found and no value.
	struct JsonResult
	{
		std::optional<JsonValue> value;
		ParseError error;
	};

	// Parses one JSON document (RFC 8259) and nothing after it. Nesting deeper than 64 levels is
	// an error, so a corrupt file can't overflow the stack.
	[[nodiscard]] JsonResult ParseJson(std::string_view text);

	// Reads and parses a file. Unlike save data, a file that doesn't exist is an error.
	[[nodiscard]] JsonResult LoadJson(const std::filesystem::path& file);

	// The text of value: each array element and object member on its own line, indented by
	// indent spaces per level, or all on one line if indent is 0. No trailing newline. The same
	// value always gives the same text.
	[[nodiscard]] std::string FormatJson(const JsonValue& value, int indent = 2);

	// Writes FormatJson(value) and a final newline to file, atomically (see WriteFileAtomic).
	// False if anything failed; the reason is logged.
	bool WriteJson(const std::filesystem::path& file, const JsonValue& value);
} // namespace engine::core
