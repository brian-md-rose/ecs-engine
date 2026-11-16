#include "core/text.hpp"

#include <charconv>
#include <fstream>
#include <iterator>
#include <system_error>

namespace engine::core
{
	namespace
	{
		template <typename T> [[nodiscard]] std::optional<T> ParseNumber(std::string_view text)
		{
			if (text.empty()) return std::nullopt;
			// from_chars doesn't take a leading '+'; allow it, as people write "+1".
			if (text.front() == '+') text.remove_prefix(1);
			T value{};
			const char* end = text.data() + text.size();
			const auto [ptr, error] = std::from_chars(text.data(), end, value);
			if (error != std::errc{} || ptr != end) return std::nullopt;
			return value;
		}
	} // namespace

	std::string FormatError(std::string_view source, const ParseError& error)
	{
		std::string text(source);
		if (error.line > 0)
		{
			text += ':' + std::to_string(error.line);
			if (error.column > 0) text += ':' + std::to_string(error.column);
		}
		text += ": ";
		text += error.message;
		return text;
	}

	std::vector<std::string_view> SplitLines(std::string_view text)
	{
		std::vector<std::string_view> lines;
		std::size_t start = 0;
		while (true)
		{
			const std::size_t end = text.find('\n', start);
			std::string_view line = text.substr(start, end == std::string_view::npos ? end : end - start);
			if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
			lines.push_back(line);
			if (end == std::string_view::npos) break;
			start = end + 1;
		}
		while (!lines.empty() && lines.back().empty())
		{
			lines.pop_back();
		}
		return lines;
	}

	std::vector<Word> SplitWords(std::string_view line)
	{
		std::vector<Word> words;
		std::size_t i = 0;
		while (i < line.size())
		{
			if (line[i] == ' ' || line[i] == '\t')
			{
				++i;
				continue;
			}
			const std::size_t start = i;
			while (i < line.size() && line[i] != ' ' && line[i] != '\t')
			{
				++i;
			}
			words.push_back({.text = line.substr(start, i - start), .column = start + 1});
		}
		return words;
	}

	std::optional<int> ParseInt(std::string_view text)
	{
		return ParseNumber<int>(text);
	}

	std::optional<float> ParseFloat(std::string_view text)
	{
		return ParseNumber<float>(text);
	}

	std::optional<std::string> ReadTextFile(const std::filesystem::path& file)
	{
		std::ifstream stream(file, std::ios::binary);
		if (!stream) return std::nullopt;
		std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		if (stream.bad()) return std::nullopt;
		return text;
	}

	std::string PathText(const std::filesystem::path& file)
	{
		const std::u8string path = file.u8string();
		return std::string(reinterpret_cast<const char*>(path.c_str()), path.size());
	}
} // namespace engine::core
