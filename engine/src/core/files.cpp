#include "core/files.hpp"

#include <fstream>
#include <string>
#include <system_error>

#include "core/log.hpp"
#include "core/text.hpp"

namespace engine::core
{
	bool WriteFileAtomic(const std::filesystem::path& file, std::string_view contents)
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
			stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
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
