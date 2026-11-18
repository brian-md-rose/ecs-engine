#include "core/json.hpp"

#include <charconv>
#include <cmath>
#include <functional>
#include <limits>
#include <set>
#include <system_error>

#include "core/assert.hpp"
#include "core/files.hpp"

namespace engine::core
{
	// ---- JsonValue ----

	std::optional<bool> JsonValue::AsBool() const noexcept
	{
		if (const bool* value = std::get_if<bool>(&storage)) return *value;
		return std::nullopt;
	}

	std::optional<std::int64_t> JsonValue::AsInt() const noexcept
	{
		if (const std::int64_t* value = std::get_if<std::int64_t>(&storage)) return *value;
		return std::nullopt;
	}

	std::optional<double> JsonValue::AsDouble() const noexcept
	{
		if (const double* value = std::get_if<double>(&storage)) return *value;
		if (const std::int64_t* value = std::get_if<std::int64_t>(&storage)) return static_cast<double>(*value);
		return std::nullopt;
	}

	std::optional<std::string_view> JsonValue::AsString() const noexcept
	{
		if (const std::string* value = std::get_if<std::string>(&storage)) return std::string_view(*value);
		return std::nullopt;
	}

	const JsonValue* JsonValue::Find(std::string_view key) const noexcept
	{
		const JsonObject* object = AsObject();
		if (object == nullptr) return nullptr;
		for (const auto& [name, value] : *object)
		{
			if (name == key) return &value;
		}
		return nullptr;
	}

	JsonValue* JsonValue::Find(std::string_view key) noexcept
	{
		return const_cast<JsonValue*>(std::as_const(*this).Find(key));
	}

	const JsonValue& JsonValue::operator[](std::size_t index) const
	{
		const JsonArray* array = AsArray();
		ENGINE_VERIFY(array != nullptr, "indexing a JSON value that is not an array");
		ENGINE_VERIFY(index < array->size(), "JSON array index out of range");
		return (*array)[index];
	}

	JsonValue& JsonValue::operator[](std::size_t index)
	{
		return const_cast<JsonValue&>(std::as_const(*this)[index]);
	}

	std::size_t JsonValue::Size() const noexcept
	{
		if (const JsonArray* array = AsArray()) return array->size();
		if (const JsonObject* object = AsObject()) return object->size();
		return 0;
	}

	JsonValue& JsonValue::Set(std::string_view key, JsonValue value)
	{
		JsonObject* object = std::get_if<JsonObject>(&storage);
		ENGINE_VERIFY(object != nullptr, "Set on a JSON value that is not an object");
		if (JsonValue* existing = Find(key))
		{
			*existing = std::move(value);
			return *existing;
		}
		return object->emplace_back(std::string(key), std::move(value)).second;
	}

	JsonValue& JsonValue::Append(JsonValue value)
	{
		JsonArray* array = std::get_if<JsonArray>(&storage);
		ENGINE_VERIFY(array != nullptr, "Append on a JSON value that is not an array");
		return array->emplace_back(std::move(value));
	}

	// ---- Parser ----

	namespace
	{
		constexpr int kMaxDepth = 64;

		void AppendUtf8(std::string& out, char32_t code)
		{
			if (code < 0x80)
			{
				out += static_cast<char>(code);
			}
			else if (code < 0x800)
			{
				out += static_cast<char>(0xC0 | (code >> 6));
				out += static_cast<char>(0x80 | (code & 0x3F));
			}
			else if (code < 0x10000)
			{
				out += static_cast<char>(0xE0 | (code >> 12));
				out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (code & 0x3F));
			}
			else
			{
				out += static_cast<char>(0xF0 | (code >> 18));
				out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
				out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (code & 0x3F));
			}
		}

		[[nodiscard]] bool IsDigit(char c) noexcept
		{
			return c >= '0' && c <= '9';
		}

		class Parser
		{
			std::string_view text;
			std::size_t pos = 0;
			ParseError error;
			bool failed = false;

		public:
			explicit Parser(std::string_view source) : text(source) {}

			[[nodiscard]] JsonResult Run()
			{
				// A UTF-8 byte order mark is allowed in front, as editors on Windows add one.
				if (text.starts_with("\xEF\xBB\xBF")) pos = 3;

				JsonValue value;
				SkipSpace();
				if (!ParseValue(value, 0)) return {.value = std::nullopt, .error = std::move(error)};
				SkipSpace();
				if (pos < text.size())
				{
					Fail(pos, "unexpected text after the end of the value");
					return {.value = std::nullopt, .error = std::move(error)};
				}
				return {.value = std::move(value), .error = {}};
			}

		private:
			// Records the first error only, at byte offset at.
			bool Fail(std::size_t at, std::string message)
			{
				if (failed) return false;
				failed = true;
				std::size_t line = 1;
				std::size_t lineStart = 0;
				for (std::size_t i = 0; i < at && i < text.size(); ++i)
				{
					if (text[i] == '\n')
					{
						++line;
						lineStart = i + 1;
					}
				}
				error = {.line = line, .column = at - lineStart + 1, .message = std::move(message)};
				return false;
			}

			void SkipSpace()
			{
				while (pos < text.size() &&
				       (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r'))
				{
					++pos;
				}
			}

			[[nodiscard]] bool Consume(std::string_view word)
			{
				if (!text.substr(pos).starts_with(word)) return false;
				pos += word.size();
				return true;
			}

			bool ParseValue(JsonValue& out, int depth)
			{
				if (pos >= text.size()) return Fail(pos, "the text ends where a value was expected");
				const char c = text[pos];
				switch (c)
				{
				case '{':
					return ParseObject(out, depth);
				case '[':
					return ParseArray(out, depth);
				case '"':
				{
					std::string value;
					if (!ParseString(value)) return false;
					out = JsonValue(std::move(value));
					return true;
				}
				case 't':
					if (!Consume("true")) return Fail(pos, "expected a value");
					out = JsonValue(true);
					return true;
				case 'f':
					if (!Consume("false")) return Fail(pos, "expected a value");
					out = JsonValue(false);
					return true;
				case 'n':
					if (!Consume("null")) return Fail(pos, "expected a value");
					out = JsonValue();
					return true;
				default:
					if (c == '-' || IsDigit(c)) return ParseNumber(out);
					return Fail(pos, "expected a value");
				}
			}

			bool ParseArray(JsonValue& out, int depth)
			{
				if (depth >= kMaxDepth) return Fail(pos, "nested more than 64 levels deep");
				++pos; // '['
				JsonArray items;
				SkipSpace();
				if (pos < text.size() && text[pos] == ']')
				{
					++pos;
					out = JsonValue(std::move(items));
					return true;
				}
				while (true)
				{
					SkipSpace();
					JsonValue item;
					if (!ParseValue(item, depth + 1)) return false;
					items.push_back(std::move(item));
					SkipSpace();
					if (pos >= text.size()) return Fail(pos, "the text ends inside an array");
					if (text[pos] == ',')
					{
						++pos;
						continue;
					}
					if (text[pos] == ']')
					{
						++pos;
						out = JsonValue(std::move(items));
						return true;
					}
					return Fail(pos, "expected ',' or ']'");
				}
			}

			bool ParseObject(JsonValue& out, int depth)
			{
				if (depth >= kMaxDepth) return Fail(pos, "nested more than 64 levels deep");
				++pos; // '{'
				JsonObject members;
				std::set<std::string, std::less<>> seen;
				SkipSpace();
				if (pos < text.size() && text[pos] == '}')
				{
					++pos;
					out = JsonValue(std::move(members));
					return true;
				}
				while (true)
				{
					SkipSpace();
					if (pos >= text.size()) return Fail(pos, "the text ends inside an object");
					if (text[pos] != '"') return Fail(pos, "expected a key in double quotes");
					const std::size_t keyAt = pos;
					std::string key;
					if (!ParseString(key)) return false;
					if (!seen.insert(key).second) return Fail(keyAt, "the key \"" + key + "\" is given twice");
					SkipSpace();
					if (pos >= text.size() || text[pos] != ':') return Fail(pos, "expected ':' after the key");
					++pos;
					SkipSpace();
					JsonValue value;
					if (!ParseValue(value, depth + 1)) return false;
					members.emplace_back(std::move(key), std::move(value));
					SkipSpace();
					if (pos >= text.size()) return Fail(pos, "the text ends inside an object");
					if (text[pos] == ',')
					{
						++pos;
						continue;
					}
					if (text[pos] == '}')
					{
						++pos;
						out = JsonValue(std::move(members));
						return true;
					}
					return Fail(pos, "expected ',' or '}'");
				}
			}

			// Four hex digits at pos, as a number.
			bool ParseHex4(char32_t& code)
			{
				if (pos + 4 > text.size()) return Fail(pos, "the text ends inside a \\u escape");
				code = 0;
				for (int i = 0; i < 4; ++i)
				{
					const char c = text[pos + static_cast<std::size_t>(i)];
					char32_t digit = 0;
					if (c >= '0' && c <= '9')
						digit = static_cast<char32_t>(c - '0');
					else if (c >= 'a' && c <= 'f')
						digit = static_cast<char32_t>(c - 'a' + 10);
					else if (c >= 'A' && c <= 'F')
						digit = static_cast<char32_t>(c - 'A' + 10);
					else
						return Fail(pos + static_cast<std::size_t>(i), "expected four hex digits after \\u");
					code = (code << 4) | digit;
				}
				pos += 4;
				return true;
			}

			bool ParseString(std::string& out)
			{
				++pos; // opening quote
				while (true)
				{
					if (pos >= text.size()) return Fail(pos, "the text ends inside a string");
					const char c = text[pos];
					if (c == '"')
					{
						++pos;
						return true;
					}
					if (static_cast<unsigned char>(c) < 0x20)
						return Fail(pos, "a line break or control character in a string needs an escape");
					if (c != '\\')
					{
						out += c;
						++pos;
						continue;
					}

					const std::size_t escapeAt = pos;
					++pos;
					if (pos >= text.size()) return Fail(pos, "the text ends inside a string");
					const char e = text[pos++];
					switch (e)
					{
					case '"':
						out += '"';
						break;
					case '\\':
						out += '\\';
						break;
					case '/':
						out += '/';
						break;
					case 'b':
						out += '\b';
						break;
					case 'f':
						out += '\f';
						break;
					case 'n':
						out += '\n';
						break;
					case 'r':
						out += '\r';
						break;
					case 't':
						out += '\t';
						break;
					case 'u':
					{
						char32_t code = 0;
						if (!ParseHex4(code)) return false;
						if (code >= 0xDC00 && code <= 0xDFFF)
							return Fail(escapeAt, "a low surrogate with no high surrogate before it");
						if (code >= 0xD800 && code <= 0xDBFF)
						{
							if (!Consume("\\u"))
								return Fail(escapeAt, "a high surrogate with no low surrogate after it");
							char32_t low = 0;
							if (!ParseHex4(low)) return false;
							if (low < 0xDC00 || low > 0xDFFF)
								return Fail(escapeAt, "a high surrogate with no low surrogate after it");
							code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
						}
						AppendUtf8(out, code);
						break;
					}
					default:
						return Fail(escapeAt, "unknown escape '\\" + std::string(1, e) + "'");
					}
				}
			}

			bool ParseNumber(JsonValue& out)
			{
				const std::size_t start = pos;
				if (text[pos] == '-') ++pos;
				if (pos >= text.size() || !IsDigit(text[pos])) return Fail(pos, "expected a digit");
				if (text[pos] == '0')
				{
					++pos;
					if (pos < text.size() && IsDigit(text[pos])) return Fail(pos, "a number can't start with 0");
				}
				else
				{
					while (pos < text.size() && IsDigit(text[pos]))
						++pos;
				}

				bool isInteger = true;
				if (pos < text.size() && text[pos] == '.')
				{
					isInteger = false;
					++pos;
					if (pos >= text.size() || !IsDigit(text[pos])) return Fail(pos, "expected a digit after '.'");
					while (pos < text.size() && IsDigit(text[pos]))
						++pos;
				}
				if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E'))
				{
					isInteger = false;
					++pos;
					if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) ++pos;
					if (pos >= text.size() || !IsDigit(text[pos])) return Fail(pos, "expected a digit in the exponent");
					while (pos < text.size() && IsDigit(text[pos]))
						++pos;
				}

				const char* first = text.data() + start;
				const char* last = text.data() + pos;
				if (isInteger)
				{
					std::int64_t value = 0;
					const auto [end, code] = std::from_chars(first, last, value);
					if (code == std::errc() && end == last)
					{
						out = JsonValue(value);
						return true;
					}
					// Too big for 64 bits: keep it as the nearest double instead.
				}
				double value = 0;
				const auto [end, code] = std::from_chars(first, last, value);
				if (code == std::errc::result_out_of_range || (code == std::errc() && !std::isfinite(value)))
				{
					return Fail(start, "the number is too large");
				}
				if (code != std::errc() || end != last) return Fail(start, "not a valid number");
				out = JsonValue(value);
				return true;
			}
		};

		// ---- Writer ----

		void WriteString(std::string& out, std::string_view value)
		{
			static constexpr char kHex[] = "0123456789abcdef";
			out += '"';
			for (const char c : value)
			{
				switch (c)
				{
				case '"':
					out += "\\\"";
					break;
				case '\\':
					out += "\\\\";
					break;
				case '\b':
					out += "\\b";
					break;
				case '\f':
					out += "\\f";
					break;
				case '\n':
					out += "\\n";
					break;
				case '\r':
					out += "\\r";
					break;
				case '\t':
					out += "\\t";
					break;
				default:
					if (static_cast<unsigned char>(c) < 0x20)
					{
						out += "\\u00";
						out += kHex[(static_cast<unsigned char>(c) >> 4) & 0xF];
						out += kHex[static_cast<unsigned char>(c) & 0xF];
					}
					else
					{
						out += c; // UTF-8 passes through
					}
				}
			}
			out += '"';
		}

		void WriteDouble(std::string& out, double value)
		{
			if (!std::isfinite(value))
			{
				out += "null";
				return;
			}
			char buffer[64];
			const auto [end, code] = std::to_chars(buffer, buffer + sizeof buffer, value);
			ENGINE_ASSERT(code == std::errc(), "a double always fits in 64 characters");
			const std::string_view digits(buffer, static_cast<std::size_t>(end - buffer));
			out += digits;
			// "2" would read back as an integer; "2.0" and "2e+20" read back as doubles.
			if (digits.find_first_of(".eE") == std::string_view::npos) out += ".0";
		}

		void NewLine(std::string& out, int indent, int level)
		{
			out += '\n';
			out.append(static_cast<std::size_t>(indent) * static_cast<std::size_t>(level), ' ');
		}

		void WriteValue(std::string& out, const JsonValue& value, int indent, int level)
		{
			if (value.IsNull())
				out += "null";
			else if (const std::optional<bool> boolean = value.AsBool())
				out += *boolean ? "true" : "false";
			else if (const std::optional<std::int64_t> integer = value.AsInt())
				out += std::to_string(*integer);
			else if (value.IsDouble())
				WriteDouble(out, *value.AsDouble());
			else if (const std::optional<std::string_view> string = value.AsString())
				WriteString(out, *string);
			else if (const JsonArray* array = value.AsArray())
			{
				if (array->empty())
				{
					out += "[]";
					return;
				}
				out += '[';
				for (std::size_t i = 0; i < array->size(); ++i)
				{
					if (i > 0) out += ',';
					if (indent > 0) NewLine(out, indent, level + 1);
					WriteValue(out, (*array)[i], indent, level + 1);
				}
				if (indent > 0) NewLine(out, indent, level);
				out += ']';
			}
			else if (const JsonObject* object = value.AsObject())
			{
				if (object->empty())
				{
					out += "{}";
					return;
				}
				out += '{';
				bool first = true;
				for (const auto& [key, member] : *object)
				{
					if (!first) out += ',';
					first = false;
					if (indent > 0) NewLine(out, indent, level + 1);
					WriteString(out, key);
					out += indent > 0 ? ": " : ":";
					WriteValue(out, member, indent, level + 1);
				}
				if (indent > 0) NewLine(out, indent, level);
				out += '}';
			}
		}
	} // namespace

	JsonResult ParseJson(std::string_view text)
	{
		return Parser(text).Run();
	}

	JsonResult LoadJson(const std::filesystem::path& file)
	{
		const std::optional<std::string> text = ReadTextFile(file);
		if (!text) return {.value = std::nullopt, .error = {.line = 0, .column = 0, .message = "can't read the file"}};
		return ParseJson(*text);
	}

	std::string FormatJson(const JsonValue& value, int indent)
	{
		ENGINE_ASSERT(indent >= 0, "indent can't be negative");
		std::string out;
		WriteValue(out, value, indent < 0 ? 0 : indent, 0);
		return out;
	}

	bool WriteJson(const std::filesystem::path& file, const JsonValue& value)
	{
		std::string text = FormatJson(value);
		text += '\n';
		return WriteFileAtomic(file, text);
	}
} // namespace engine::core
