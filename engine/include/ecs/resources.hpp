#pragma once
#include <algorithm>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.hpp"

namespace engine::ecs
{
	// Anything a world can hold one of: a class type, not const. Unlike components, resources
	// needn't be movable: each one is built in place and never relocated.
	template <typename T>
	concept Resource = std::is_class_v<T> && !std::is_const_v<T>;

	// One value of each type, shared by every system: controls, game state, the renderer, and
	// later the maze and score. Looked up by type, so the engine needs no knowledge of the
	// game's types. See docs/decisions/0001-shared-game-data.md.
	//
	// References returned by Emplace/Get stay valid until that resource is removed: adding
	// other resources never moves existing ones. Resources are destroyed in reverse order of
	// insertion, so a resource may refer to any resource added before it.
	class Resources
	{
		struct IResource
		{
			virtual ~IResource() = default;
		};

		template <typename T> struct Holder final : IResource
		{
			T value;

			template <typename... Args> explicit Holder(Args&&... args) : value(std::forward<Args>(args)...) {}
		};

		std::unordered_map<std::type_index, std::unique_ptr<IResource>> items;
		std::vector<std::type_index> order; // insertion order, for teardown

		template <Resource T> T* Find() const noexcept
		{
			const auto it = items.find(std::type_index(typeid(T)));
			// Safe: the entry for typeid(T) is only ever created by Emplace<T> as a Holder<T>.
			return it == items.end() ? nullptr : &static_cast<Holder<T>*>(it->second.get())->value;
		}

	public:
		Resources() = default;
		~Resources();

		Resources(const Resources&) = delete;
		Resources& operator=(const Resources&) = delete;
		Resources(Resources&&) = delete;
		Resources& operator=(Resources&&) = delete;

		// Constructs T in place from args. There is one value per type: adding a type that is
		// already present asserts, and in Release returns the existing value unchanged.
		template <Resource T, typename... Args> T& Emplace(Args&&... args)
		{
			if (T* existing = Find<T>())
			{
				ENGINE_ASSERT(false, "resource already present; use Get to change it");
				return *existing;
			}

			auto holder = std::make_unique<Holder<T>>(std::forward<Args>(args)...);
			T& value = holder->value;
			order.emplace_back(typeid(T));
			items.emplace(typeid(T), std::move(holder));
			return value;
		}

		// The resource of type T. Always checked: a missing resource reports the type's name
		// and aborts, in Release too. Use TryGet where a resource is optional.
		template <Resource T> [[nodiscard]] T& Get()
		{
			T* value = Find<T>();
			ENGINE_VERIFY(value != nullptr, typeid(T).name());
			return *value;
		}

		template <Resource T> [[nodiscard]] const T& Get() const
		{
			const T* value = Find<T>();
			ENGINE_VERIFY(value != nullptr, typeid(T).name());
			return *value;
		}

		// nullptr if T is absent.
		template <Resource T> [[nodiscard]] T* TryGet() noexcept { return Find<T>(); }
		template <Resource T> [[nodiscard]] const T* TryGet() const noexcept { return Find<T>(); }

		template <Resource T> [[nodiscard]] bool Has() const noexcept { return Find<T>() != nullptr; }

		// Destroys the resource of type T, if present. References to it become invalid.
		template <Resource T> void Remove()
		{
			const std::type_index type(typeid(T));
			if (items.erase(type) > 0)
			{
				order.erase(std::ranges::find(order, type));
			}
		}

		[[nodiscard]] std::size_t Count() const noexcept { return items.size(); }
	};

	inline Resources::~Resources()
	{
		// unordered_map destroys its entries in no particular order; go newest first instead.
		for (auto it = order.rbegin(); it != order.rend(); ++it)
		{
			items.erase(*it);
		}
	}
} // namespace engine::ecs
