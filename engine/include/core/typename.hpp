#pragma once

#include <initializer_list>
#include <string_view>

// A type's name as written in the source, for diagnostics such as the debug panel. Read from the
// compiler's own spelling of a template function's signature, so it needs no RTTI, makes no
// allocation and is the same on MSVC, GCC and Clang (typeid().name() is mangled on GCC and Clang).
namespace engine::core
{
	namespace detail
	{
		template <typename T> [[nodiscard]] constexpr std::string_view Signature() noexcept
		{
#if defined(__clang__) || defined(__GNUC__)
			return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
			return __FUNCSIG__;
#else
			return {};
#endif
		}

		// The template argument inside a Signature: "T = game::Pellet" on GCC and Clang,
		// "<struct game::Pellet>(void)" on MSVC.
		[[nodiscard]] constexpr std::string_view TemplateArgument(std::string_view signature) noexcept
		{
#if defined(__clang__) || defined(__GNUC__)
			const std::size_t start = signature.find("T = ");
			if (start == std::string_view::npos) return {};
			const std::size_t begin = start + 4;
			const std::size_t end = signature.find_first_of(";]", begin);
			return signature.substr(begin, end - begin);
#else
			const std::size_t begin = signature.find("Signature<");
			const std::size_t end = signature.rfind(">(void)");
			if (begin == std::string_view::npos || end == std::string_view::npos) return {};
			std::string_view argument = signature.substr(begin + 10, end - begin - 10);
			for (const std::string_view keyword : {"struct ", "class ", "enum ", "union "})
			{
				if (argument.starts_with(keyword)) argument.remove_prefix(keyword.size());
			}
			return argument;
#endif
		}
	} // namespace detail

	// The full name, with namespaces: "game::Pellet". Empty on an unknown compiler.
	template <typename T> [[nodiscard]] constexpr std::string_view QualifiedTypeName() noexcept
	{
		return detail::TemplateArgument(detail::Signature<T>());
	}

	// The name without its namespaces: "Pellet".
	template <typename T> [[nodiscard]] constexpr std::string_view TypeName() noexcept
	{
		const std::string_view name = QualifiedTypeName<T>();
		const std::size_t colons = name.rfind("::");
		return colons == std::string_view::npos ? name : name.substr(colons + 2);
	}
} // namespace engine::core
