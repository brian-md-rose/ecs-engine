#pragma once
#include <concepts>
#include <type_traits>

namespace engine::ecs
{
	template <typename T>
	concept Component = std::is_class_v<T> &&  // a struct/class, not int, a pointer, etc.
	                    !std::is_const_v<T> && // a const component couldn't be updated in place
	                    std::movable<T>;       // Remove() uses swap-and-pop
}