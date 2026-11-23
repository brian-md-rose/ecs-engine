#include "editor/assetthumbnails.hpp"

#include <utility>

#include "core/text.hpp"
#include "graphics/pixelsheet.hpp"
#include "graphics/sheetmanifest.hpp"

namespace engine::editor
{
	AssetThumbnails::Sheet& AssetThumbnails::Load(const std::filesystem::path& file)
	{
		const auto found = sheets.find(file);
		if (found != sheets.end()) return found->second;
		Sheet& sheet = sheets[file];
		graphics::PixelSheetResult result = graphics::LoadSheetManifest(file);
		if (!result.sheet)
		{
			sheet.error = core::FormatError(core::PathText(file.filename()), result.error);
			return sheet;
		}
		for (const graphics::SheetSprite& sprite : result.sheet->sprites)
		{
			const graphics::TextureId id = renderer->CreateTexture(sprite.image);
			if (id == graphics::kNoTexture) continue;
			sheet.textures[sprite.name] = id;
			sheet.pictures[sprite.name] = {.texture = renderer->GetTextureHandle(id),
			                               .width = sprite.image.Width(),
			                               .height = sprite.image.Height(),
			                               .frames = sprite.frames};
		}
		return sheet;
	}

	AssetThumbnails::Picture AssetThumbnails::Get(const std::filesystem::path& file, const std::string& name)
	{
		if (renderer == nullptr) return {};
		const Sheet& sheet = Load(file);
		const auto found = sheet.pictures.find(name);
		return found == sheet.pictures.end() ? Picture{} : found->second;
	}

	const std::string& AssetThumbnails::Error(const std::filesystem::path& file) const
	{
		static const std::string none;
		const auto found = sheets.find(file);
		return found == sheets.end() ? none : found->second.error;
	}

	void AssetThumbnails::Forget()
	{
		dropPending = true;
	}

	void AssetThumbnails::SetRenderer(graphics::Renderer* rendererToUse)
	{
		if (dropPending && renderer != nullptr)
			for (const auto& entry : sheets)
				for (const auto& texture : entry.second.textures)
					renderer->DestroyTexture(texture.second);
		if (dropPending) sheets.clear();
		dropPending = false;
		renderer = rendererToUse;
	}
} // namespace engine::editor
