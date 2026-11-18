#include "core/savedata.hpp"

#include "core/log.hpp"
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

#include "core/assert.hpp"

namespace engine::core
{
	namespace
	{
		constexpr std::string_view kSpaces = " \t";

		[[nodiscard]] bool IsBlankOrComment(std::string_view line)
		{
			const std::size_t first = line.find_first_not_of(kSpaces);
			return first == std::string_view::npos || line[first] == '#';
		}

		[[nodiscard]] std::string_view Trim(std::string_view text)
		{
			const std::size_t first = text.find_first_not_of(kSpaces);
			if (first == std::string_view::npos) return {};
			const std::size_t last = text.find_last_not_of(kSpaces);
			return text.substr(first, last - first + 1);
		}

		[[nodiscard]] bool IsValidKey(std::string_view key)
		{
			return !key.empty() && key.front() != '#' && key.find_first_of(" \t\r\n") == std::string_view::npos;
		}

		[[nodiscard]] SaveDataResult Fail(std::size_t line, std::size_t column, std::string message)
		{
			return {.data = std::nullopt, .error = {.line = line, .column = column, .message = std::move(message)}};
		}
	} // namespace

	std::optional<std::string_view> SaveData::Get(std::string_view key) const
	{
		const auto it = values.find(key);
		if (it == values.end()) return std::nullopt;
		return std::string_view(it->second);
	}

	std::optional<int> SaveData::GetInt(std::string_view key) const
	{
		const std::optional<std::string_view> value = Get(key);
		if (!value) return std::nullopt;
		return ParseInt(*value);
	}

	void SaveData::Set(std::string_view key, std::string_view value)
	{
		ENGINE_VERIFY(IsValidKey(key), "a save key must be one word, not starting with '#'");
		ENGINE_ASSERT(value.find_first_of("\r\n") == std::string_view::npos, "a saved value must fit on one line");
		values.insert_or_assign(std::string(key), std::string(Trim(value)));
	}

	void SaveData::SetInt(std::string_view key, int value)
	{
		Set(key, std::to_string(value));
	}

	std::string SaveData::Format() const
	{
		std::string text;
		for (const auto& [key, value] : values)
		{
			text += key;
			text += ' ';
			text += value;
			text += '\n';
		}
		return text;
	}

	SaveDataResult ParseSaveData(std::string_view text)
	{
		SaveData data;
		const std::vector<std::string_view> lines = SplitLines(text);
		for (std::size_t index = 0; index < lines.size(); ++index)
		{
			const std::string_view line = lines[index];
			if (IsBlankOrComment(line)) continue;

			const std::size_t keyStart = line.find_first_not_of(kSpaces);
			const std::size_t keyEnd = line.find_first_of(kSpaces, keyStart);
			const std::string_view key = line.substr(keyStart, keyEnd - keyStart);
			const std::string_view value =
			    keyEnd == std::string_view::npos ? std::string_view{} : Trim(line.substr(keyEnd));
			if (value.empty())
			{
				return Fail(index + 1, keyStart + 1, "'" + std::string(key) + "' has no value");
			}
			data.Set(key, value);
		}
		return {.data = std::move(data), .error = {}};
	}

	SaveDataResult LoadSaveData(const std::filesystem::path& file)
	{
		std::error_code error;
		if (!std::filesystem::exists(file, error) && !error) return {.data = SaveData{}, .error = {}};

		const std::optional<std::string> text = ReadTextFile(file);
		if (!text) return Fail(0, 0, "can't read the file");
		return ParseSaveData(*text);
	}

	bool WriteSaveData(const std::filesystem::path& file, const SaveData& data)
	{
		std::error_code error;
		if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), error);
		if (error)
		{
			LogError(PathText(file) + ": can't make the folder: " + error.message());
			return false;
		}

		std::filesystem::path temporary = file;
		temporary += ".tmp";
		{
			std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
			stream << data.Format();
			stream.flush();
			if (!stream)
			{
				LogError(PathText(temporary) + ": can't write the file");
				std::filesystem::remove(temporary, error);
				return false;
			}
		}

		std::filesystem::rename(temporary, file, error); // replaces file if it exists
		if (error)
		{
			LogError(PathText(file) + ": can't replace the file: " + error.message());
			std::filesystem::remove(temporary, error);
			return false;
		}
		return true;
	}
} // namespace engine::core
