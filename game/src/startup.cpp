#include "startup.hpp"

#include <optional>
#include <system_error>

#include "core/json.hpp"
#include "core/log.hpp"
#include "core/text.hpp"

namespace game
{
	std::string ReadStartupScene(const std::filesystem::path& manifest)
	{
		std::error_code error;
		if (!std::filesystem::exists(manifest, error)) return {};

		const engine::core::JsonResult parsed = engine::core::LoadJson(manifest);
		if (!parsed.value || !parsed.value->IsObject())
		{
			engine::core::LogWarning(engine::core::PathText(manifest) + " could not be read; ignoring it.");
			return {};
		}
		const engine::core::JsonValue* field = parsed.value->Find("startupScene");
		const std::optional<std::string_view> name = field != nullptr ? field->AsString() : std::nullopt;
		if (!name || name->empty()) return {};
		if (name->find_first_of("/\\:") != std::string_view::npos || name->find("..") != std::string_view::npos)
		{
			engine::core::LogWarning(engine::core::PathText(manifest) + ": startupScene \"" + std::string(*name) +
			                         "\" is not a plain scene name; ignoring it.");
			return {};
		}
		return std::string(*name);
	}
} // namespace game
