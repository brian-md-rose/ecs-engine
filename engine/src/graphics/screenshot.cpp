#include "graphics/screenshot.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <system_error>

#ifndef ENGINE_SCREENSHOT_DIR
#define ENGINE_SCREENSHOT_DIR ""
#endif

namespace engine::graphics
{
	std::string ScreenshotFileName(const ScreenshotTime& time, int suffix)
	{
		std::array<char, 64> text{};
		const int length =
		    std::snprintf(text.data(), text.size(), "screenshot-%04d%02d%02d-%02d%02d%02d",
		                  std::clamp(time.year, 0, 9999), std::clamp(time.month, 1, 12), std::clamp(time.day, 1, 31),
		                  std::clamp(time.hour, 0, 23), std::clamp(time.minute, 0, 59), std::clamp(time.second, 0, 60));
		std::string name(text.data(), static_cast<std::size_t>(std::max(length, 0)));
		if (suffix > 1) name += "-" + std::to_string(suffix);
		return name + ".png";
	}

	std::filesystem::path UniqueScreenshotPath(const std::filesystem::path& folder, const ScreenshotTime& time)
	{
		// A bounded search: a folder holding this many files for one second is broken, not busy.
		constexpr int kMaxSuffix = 1000;
		for (int suffix = 0; suffix <= kMaxSuffix; ++suffix)
		{
			std::filesystem::path candidate = folder / ScreenshotFileName(time, suffix);
			std::error_code error;
			if (!std::filesystem::exists(candidate, error)) return candidate;
		}
		return folder / ScreenshotFileName(time, kMaxSuffix);
	}

	std::filesystem::path ScreenshotDirectory(const std::filesystem::path& saveDirectory)
	{
		return ScreenshotDirectory(saveDirectory, ENGINE_SCREENSHOT_DIR);
	}

	std::filesystem::path ScreenshotDirectory(const std::filesystem::path& saveDirectory,
	                                          const std::filesystem::path& repositoryDirectory)
	{
		std::error_code error;
		if (!repositoryDirectory.empty() && std::filesystem::is_directory(repositoryDirectory, error))
		{
			return repositoryDirectory;
		}
		if (saveDirectory.empty()) return {};
		return saveDirectory / "screenshots";
	}

	Image CropImage(const Image& image, PixelRect rect)
	{
		const int left = std::max(rect.x, 0);
		const int top = std::max(rect.y, 0);
		const int right = std::min(rect.x + rect.width, image.Width());
		const int bottom = std::min(rect.y + rect.height, image.Height());
		if (right <= left || bottom <= top) return {};
		Image result(right - left, bottom - top);
		for (int y = top; y < bottom; ++y)
		{
			for (int x = left; x < right; ++x)
			{
				result.Set(x - left, y - top, image.At(x, y));
			}
		}
		return result;
	}

	Image ResizeImage(const Image& image, int width, int height)
	{
		if (image.IsEmpty() || width <= 0 || height <= 0) return {};
		const int sourceWidth = image.Width();
		const int sourceHeight = image.Height();
		Image result(width, height);
		for (int y = 0; y < height; ++y)
		{
			// The source rows this row covers, always at least one.
			const int top = static_cast<int>(static_cast<long long>(y) * sourceHeight / height);
			const int bottom =
			    std::max(top + 1, static_cast<int>(static_cast<long long>(y + 1) * sourceHeight / height));
			for (int x = 0; x < width; ++x)
			{
				const int left = static_cast<int>(static_cast<long long>(x) * sourceWidth / width);
				const int right =
				    std::max(left + 1, static_cast<int>(static_cast<long long>(x + 1) * sourceWidth / width));
				long long r = 0;
				long long g = 0;
				long long b = 0;
				long long a = 0;
				for (int sy = top; sy < bottom; ++sy)
				{
					for (int sx = left; sx < right; ++sx)
					{
						const Rgba pixel = image.At(sx, sy);
						r += pixel.r;
						g += pixel.g;
						b += pixel.b;
						a += pixel.a;
					}
				}
				const long long count = static_cast<long long>(bottom - top) * (right - left);
				const auto average = [count](long long sum)
				{
					return static_cast<std::uint8_t>((sum + count / 2) / count);
				};
				result.Set(x, y, Rgba{average(r), average(g), average(b), average(a)});
			}
		}
		return result;
	}
} // namespace engine::graphics
