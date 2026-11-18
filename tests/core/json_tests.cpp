#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

#include "core/files.hpp"
#include "core/json.hpp"

namespace
{
	using engine::core::FormatJson;
	using engine::core::JsonArray;
	using engine::core::JsonObject;
	using engine::core::JsonResult;
	using engine::core::JsonValue;
	using engine::core::ParseJson;

	struct TempFolder
	{
		std::filesystem::path path;

		explicit TempFolder(const char* name) : path(std::filesystem::temp_directory_path() / name)
		{
			std::filesystem::remove_all(path);
		}
		~TempFolder() { std::filesystem::remove_all(path); }

		TempFolder(const TempFolder&) = delete;
		TempFolder& operator=(const TempFolder&) = delete;
		TempFolder(TempFolder&&) = delete;
		TempFolder& operator=(TempFolder&&) = delete;
	};

	// The value of text, which must parse.
	JsonValue Parse(std::string_view text)
	{
		JsonResult result = ParseJson(text);
		REQUIRE(result.value.has_value());
		return *std::move(result.value);
	}

	// Parses text, which must be rejected, and checks where and why.
	void CheckError(std::string_view text, std::size_t line, std::size_t column, std::string_view messagePart)
	{
		const JsonResult result = ParseJson(text);
		INFO("text: " << text);
		REQUIRE_FALSE(result.value.has_value());
		CHECK(result.error.line == line);
		CHECK(result.error.column == column);
		CHECK(result.error.message.find(messagePart) != std::string::npos);
	}

	// A save-shaped document: a profile with captains holding soldiers, and a map graph.
	constexpr const char* kProfile = R"({
  "version": 3,
  "seed": 9223372036854775807,
  "name": "Ada \"the Bold\"",
  "captains": [
    {
      "id": 1,
      "name": "Mira",
      "soldiers": [
        {"id": 10, "hp": 12.5, "alive": true},
        {"id": 11, "hp": 0.1, "alive": false}
      ]
    },
    {"id": 2, "name": "Tor", "soldiers": []}
  ],
  "map": {"nodes": [0, 1, 2], "edges": [[0, 1], [1, 2]], "start": null},
  "empty": {}
})";
} // namespace

TEST_CASE("ParseJson reads each kind of value", "[core][json]")
{
	CHECK(Parse("null").IsNull());
	CHECK(Parse("true").AsBool() == true);
	CHECK(Parse("false").AsBool() == false);
	CHECK(Parse("-17").AsInt() == -17);
	CHECK(Parse("2.5").AsDouble() == 2.5);
	CHECK(Parse("1e3").AsDouble() == 1000.0);
	CHECK(Parse("\"hi\"").AsString() == "hi");
	CHECK(Parse("[1, 2]").Size() == 2);
	CHECK(Parse("{\"a\": 1}").Find("a")->AsInt() == 1);
	CHECK(Parse("  \n\t[ ]  ").Size() == 0);
}

TEST_CASE("Integers and doubles stay apart", "[core][json]")
{
	const JsonValue integer = Parse("3");
	CHECK(integer.IsInt());
	CHECK_FALSE(integer.IsDouble());
	CHECK(integer.AsDouble() == 3.0); // AsDouble accepts both

	const JsonValue real = Parse("3.0");
	CHECK(real.IsDouble());
	CHECK_FALSE(real.AsInt().has_value());

	CHECK(Parse("1E2").IsDouble());
	CHECK(Parse("-0").IsInt());
}

TEST_CASE("int64 extremes round-trip exactly", "[core][json]")
{
	constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
	constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
	CHECK(Parse("9223372036854775807").AsInt() == kMax);
	CHECK(Parse("-9223372036854775808").AsInt() == kMin);
	CHECK(FormatJson(JsonValue(kMax)) == "9223372036854775807");
	CHECK(FormatJson(JsonValue(kMin)) == "-9223372036854775808");

	// One past the end is no longer an integer, but is still a number.
	const JsonValue big = Parse("9223372036854775808");
	CHECK(big.IsDouble());
}

TEST_CASE("Doubles print as the shortest text that reads back the same", "[core][json]")
{
	CHECK(FormatJson(JsonValue(0.1)) == "0.1");
	CHECK(FormatJson(JsonValue(2.0)) == "2.0"); // stays a double when read again
	CHECK(FormatJson(JsonValue(-1.5e-7)) == "-1.5e-07");
	for (const double value : {0.1, 1.0 / 3.0, 1e300, 5e-324, 123456789.125, -0.0})
	{
		const JsonValue back = Parse(FormatJson(JsonValue(value)));
		INFO("value: " << value);
		REQUIRE(back.IsDouble());
		CHECK(*back.AsDouble() == value);
	}
	CHECK(FormatJson(JsonValue(std::numeric_limits<double>::infinity())) == "null");
	CHECK(FormatJson(JsonValue(std::numeric_limits<double>::quiet_NaN())) == "null");
}

TEST_CASE("Strings handle escapes, UTF-8 and surrogate pairs", "[core][json]")
{
	CHECK(Parse(R"("a\"b\\c\/d\b\f\n\r\t")").AsString() == "a\"b\\c/d\b\f\n\r\t");
	CHECK(Parse(R"("\u0041\u00e9\u20AC")").AsString() == "A\xC3\xA9\xE2\x82\xAC");
	CHECK(Parse(R"("\uD83D\uDE00")").AsString() == "\xF0\x9F\x98\x80"); // U+1F600
	CHECK(Parse("\"\xC3\xA9\"").AsString() == "\xC3\xA9");              // raw UTF-8 passes through

	CHECK(FormatJson(JsonValue("tab\there \"q\" \\ \x01")) == R"("tab\there \"q\" \\ \u0001")");
	CHECK(FormatJson(JsonValue("\xC3\xA9")) == "\"\xC3\xA9\""); // not escaped
}

TEST_CASE("Objects keep key order", "[core][json]")
{
	const JsonValue value = Parse(R"({"z": 1, "a": 2, "m": 3})");
	const JsonObject* object = value.AsObject();
	REQUIRE(object != nullptr);
	REQUIRE(object->size() == 3);
	CHECK((*object)[0].first == "z");
	CHECK((*object)[1].first == "a");
	CHECK((*object)[2].first == "m");
	CHECK(FormatJson(value, 0) == R"({"z":1,"a":2,"m":3})");
}

TEST_CASE("Parse errors give the right line and column", "[core][json]")
{
	CheckError("", 1, 1, "ends");
	CheckError("   ", 1, 4, "ends");
	CheckError("[1, 2", 1, 6, "ends inside an array");
	CheckError("[1 2]", 1, 4, "expected ',' or ']'");
	CheckError("{\"a\": 1,\n \"b\" 2}", 2, 6, "expected ':'");
	CheckError("{\"a\": 1,\n\n  x: 2}", 3, 3, "expected a key");
	CheckError("{\"a\": 1, \"a\": 2}", 1, 10, "given twice");
	CheckError("{\"a\": 1}\n  x", 2, 3, "after the end");
	CheckError("nul", 1, 1, "expected a value");
	CheckError("\"abc", 1, 5, "ends inside a string");
	CheckError("\"a\nb\"", 1, 3, "control character");
	CheckError("\"\\q\"", 1, 2, "unknown escape");
	CheckError("\"\\u12G4\"", 1, 6, "hex digits");
	CheckError("\"\\uD800\"", 1, 2, "high surrogate");
	CheckError("\"\\uDC00\"", 1, 2, "low surrogate");
	CheckError("01", 1, 2, "start with 0");
	CheckError("-", 1, 2, "digit");
	CheckError("1.", 1, 3, "digit after '.'");
	CheckError("1e", 1, 3, "exponent");
	CheckError("1e999", 1, 1, "too large");
	CheckError("[1,]", 1, 4, "expected a value");
}

TEST_CASE("Nesting is capped at 64 levels", "[core][json]")
{
	const auto nested = [](int depth)
	{
		return std::string(static_cast<std::size_t>(depth), '[') + std::string(static_cast<std::size_t>(depth), ']');
	};
	CHECK(ParseJson(nested(64)).value.has_value());

	const JsonResult tooDeep = ParseJson(nested(65));
	REQUIRE_FALSE(tooDeep.value.has_value());
	CHECK(tooDeep.error.message.find("64") != std::string::npos);

	// A hostile file must fail cleanly rather than overflow the stack.
	CHECK_FALSE(ParseJson(std::string(100000, '[')).value.has_value());
}

TEST_CASE("A byte order mark in front is skipped", "[core][json]")
{
	CHECK(Parse("\xEF\xBB\xBF[1]").Size() == 1);
}

TEST_CASE("A Babel-shaped profile round-trips byte for byte", "[core][json]")
{
	const JsonValue profile = Parse(kProfile);
	const std::string formatted = FormatJson(profile);

	// Lists of captains with nested soldiers, and a map graph, come back as they went in.
	const JsonValue* captains = profile.Find("captains");
	REQUIRE(captains != nullptr);
	CHECK((*captains)[0].Find("soldiers")->Size() == 2);
	CHECK((*captains)[0].Find("soldiers")->operator[](0).Find("hp")->AsDouble() == 12.5);
	CHECK(profile.Find("seed")->AsInt() == std::numeric_limits<std::int64_t>::max());
	CHECK(profile.Find("map")->Find("start")->IsNull());

	// format -> parse -> format gives identical text, and the same value.
	const JsonValue again = Parse(formatted);
	CHECK(again == profile);
	CHECK(FormatJson(again) == formatted);

	// The text we wrote reads back byte for byte as the writer's own output.
	CHECK(formatted.find("\"seed\": 9223372036854775807") != std::string::npos);
	CHECK(formatted.find("\"soldiers\": []") != std::string::npos);
	CHECK(formatted.find("\"empty\": {}") != std::string::npos);
}

TEST_CASE("FormatJson indents, or goes on one line with indent 0", "[core][json]")
{
	const JsonValue value = Parse(R"({"a": [1, {"b": null}], "c": "x"})");
	CHECK(FormatJson(value, 0) == R"({"a":[1,{"b":null}],"c":"x"})");
	CHECK(FormatJson(value, 2) == "{\n"
	                              "  \"a\": [\n"
	                              "    1,\n"
	                              "    {\n"
	                              "      \"b\": null\n"
	                              "    }\n"
	                              "  ],\n"
	                              "  \"c\": \"x\"\n"
	                              "}");
	CHECK(FormatJson(Parse("[]")) == "[]");
	CHECK(FormatJson(Parse("{}")) == "{}");
}

TEST_CASE("JsonValue can be built and changed", "[core][json]")
{
	JsonValue root = JsonValue::Object();
	root.Set("name", "Mira");
	root.Set("level", 3);
	root.Set("big", std::int64_t{1} << 40);
	root.Set("ratio", 0.5);
	root.Set("ok", true);
	JsonValue& list = root.Set("tags", JsonValue::Array());
	list.Append("a");
	list.Append(2);

	root.Set("level", 4); // replaces in place, keeping its position
	CHECK(root.Find("level")->AsInt() == 4);
	CHECK(root.Size() == 6);
	CHECK(root.Find("missing") == nullptr);
	CHECK(root.AsObject()->at(1).first == "level");

	CHECK(FormatJson(root, 0) ==
	      R"({"name":"Mira","level":4,"big":1099511627776,"ratio":0.5,"ok":true,"tags":["a",2]})");

	// Wrong-kind reads answer nothing, and don't crash.
	CHECK_FALSE(root.AsInt().has_value());
	CHECK_FALSE(root.Find("name")->AsInt().has_value());
	CHECK(Parse("5").Find("x") == nullptr);
	CHECK(Parse("5").Size() == 0);
}

TEST_CASE("WriteJson and LoadJson round-trip through a file", "[core][json]")
{
	const TempFolder folder("engine_json_tests");
	const std::filesystem::path file = folder.path / "nested" / "profile.json";

	const JsonValue profile = Parse(kProfile);
	REQUIRE(engine::core::WriteJson(file, profile));
	CHECK_FALSE(std::filesystem::exists(file.string() + ".tmp"));

	// The file ends with a newline, and what is in it is exactly the formatted text.
	std::ifstream stream(file, std::ios::binary);
	std::stringstream contents;
	contents << stream.rdbuf();
	CHECK(contents.str() == FormatJson(profile) + "\n");

	const JsonResult loaded = engine::core::LoadJson(file);
	REQUIRE(loaded.value.has_value());
	CHECK(*loaded.value == profile);
}

TEST_CASE("LoadJson reports a missing or broken file", "[core][json]")
{
	const TempFolder folder("engine_json_tests_bad");
	const JsonResult missing = engine::core::LoadJson(folder.path / "nope.json");
	CHECK_FALSE(missing.value.has_value());

	std::filesystem::create_directories(folder.path);
	{
		std::ofstream stream(folder.path / "bad.json", std::ios::binary);
		stream << "{\n  \"a\": 1,\n  \"b\": oops\n}\n";
	}
	const JsonResult broken = engine::core::LoadJson(folder.path / "bad.json");
	REQUIRE_FALSE(broken.value.has_value());
	CHECK(broken.error.line == 3);
	CHECK(broken.error.column == 8);
}

TEST_CASE("JsonValue integer constructors", "[core][json]")
{
	// A character is not a number: JsonValue('a') doesn't compile.
	static_assert(!std::is_constructible_v<JsonValue, char>);
	CHECK(JsonValue(std::uint8_t{200}).AsInt() == 200);
	CHECK(JsonValue(std::size_t{7}).AsInt() == 7);
	CHECK(JsonValue(std::int64_t{-5}).AsInt() == -5);
	CHECK(JsonValue(static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())).AsInt() ==
	      std::numeric_limits<std::int64_t>::max());
	CHECK(Parse("-0").AsInt() == 0); // integers have no negative zero
}

TEST_CASE("WriteFileAtomic makes the folder and leaves no temporary file", "[core][files]")
{
	const TempFolder folder("engine_files_tests");
	const std::filesystem::path file = folder.path / "a" / "b" / "out.txt";

	REQUIRE(engine::core::WriteFileAtomic(file, "first"));
	REQUIRE(engine::core::WriteFileAtomic(file, "second")); // replaces
	CHECK_FALSE(std::filesystem::exists(file.string() + ".tmp"));

	std::ifstream stream(file, std::ios::binary);
	std::stringstream contents;
	contents << stream.rdbuf();
	CHECK(contents.str() == "second");
}

TEST_CASE("WriteFileAtomic fails cleanly when the folder can't be made", "[core][files]")
{
	const TempFolder folder("engine_files_tests_fail");
	std::filesystem::create_directories(folder.path);
	const std::filesystem::path blocker = folder.path / "blocker";
	{
		std::ofstream stream(blocker);
		stream << "a file, not a folder";
	}
	// A path that goes through a file can't be made into a folder.
	CHECK_FALSE(engine::core::WriteFileAtomic(blocker / "sub" / "out.txt", "x"));
	CHECK(std::filesystem::is_regular_file(blocker));
}
