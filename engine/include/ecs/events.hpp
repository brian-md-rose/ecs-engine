#pragma once
#include <concepts>
#include <cstddef>
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
	struct World;

	// Anything that can be published: a copyable class type, not const. Events are small values
	// ("this pellet was eaten"), copied once into the queue and once more on delivery.
	template <typename T>
	concept Event = std::is_class_v<T> && !std::is_const_v<T> && std::copy_constructible<T>;

	// Reacts to one event. A plain function, like a system: state it keeps belongs in a resource.
	template <Event T> using EventHandler = void (*)(World& world, const T& event);

	// A typed publish/subscribe queue, so systems can announce what happened without knowing who
	// cares (scoring, audio and the HUD all react to "pellet eaten"). See
	// docs/decisions/0002-events.md.
	//
	//     world.events.Subscribe<PelletEaten>(AddPelletScore);   // setup
	//     world.events.Publish(PelletEaten{tile});               // in a system
	//
	// Publishing only queues the event. Deliver() hands every queued event to its type's
	// handlers, in the order the events were published; each event goes to its handlers in the
	// order they subscribed. Events a handler publishes are queued behind the rest and delivered
	// in the same call, so one Deliver() leaves the queue empty. Events of a type nobody
	// subscribes to are dropped.
	//
	// The Scheduler calls Deliver() at the end of each phase, after applying world.commands, so
	// handlers see the entities the phase created and destroyed.
	class EventBus
	{
		struct IChannel
		{
			IChannel() = default;
			IChannel(const IChannel&) = delete;
			IChannel& operator=(const IChannel&) = delete;
			IChannel(IChannel&&) = delete;
			IChannel& operator=(IChannel&&) = delete;
			virtual ~IChannel() = default;

			// Hands the oldest undelivered event of this type to every handler.
			virtual void DeliverNext(World& world) = 0;
			virtual void Clear() noexcept = 0;
		};

		// The handlers and pending events of one type.
		template <Event T> struct Channel final : IChannel
		{
			std::vector<EventHandler<T>> handlers;
			std::vector<T> pending;
			std::size_t next = 0; // the first pending event not yet delivered

			void DeliverNext(World& world) override
			{
				// A copy: a handler may publish another T, and growing pending would leave a
				// reference into it dangling.
				const T event = pending[next++];
				for (const EventHandler<T> handler : handlers)
				{
					handler(world, event);
				}
			}

			void Clear() noexcept override
			{
				pending.clear();
				next = 0;
			}
		};

		std::unordered_map<std::type_index, std::unique_ptr<IChannel>> channels;
		std::vector<IChannel*> queue; // the channel of every pending event, in publish order
		bool delivering = false;

		template <Event T> Channel<T>& GetChannel()
		{
			std::unique_ptr<IChannel>& channel = channels[typeid(T)];
			if (!channel) channel = std::make_unique<Channel<T>>();
			// Safe: the entry for typeid(T) is only ever created here as a Channel<T>.
			return static_cast<Channel<T>&>(*channel);
		}

		template <Event T> const Channel<T>* FindChannel() const
		{
			const auto it = channels.find(std::type_index(typeid(T)));
			return it == channels.end() ? nullptr : static_cast<const Channel<T>*>(it->second.get());
		}

	public:
		// A handler publishing in a loop would never let Deliver() return; past this many events
		// in one call it aborts instead.
		static constexpr std::size_t kMaxEventsPerDelivery = 1 << 16;

		EventBus() = default;
		~EventBus() = default;

		EventBus(const EventBus&) = delete;
		EventBus& operator=(const EventBus&) = delete;
		EventBus(EventBus&&) = delete;
		EventBus& operator=(EventBus&&) = delete;

		// Adds handler for events of type T, after the handlers already there. Do it during
		// setup: subscribing from inside a handler asserts.
		template <Event T> void Subscribe(EventHandler<T> handler)
		{
			ENGINE_VERIFY(handler != nullptr, "cannot subscribe a null event handler");
			ENGINE_ASSERT(!delivering, "cannot subscribe while events are being delivered; subscribe during setup");
			GetChannel<T>().handlers.push_back(handler);
		}

		// Queues event for the next Deliver(). Allowed anywhere: in systems, in handlers, and
		// during setup.
		template <Event T> void Publish(T event)
		{
			Channel<T>& channel = GetChannel<T>();
			if (channel.handlers.empty()) return; // nobody listens: nothing to deliver
			channel.pending.push_back(std::move(event));
			queue.push_back(&channel);
		}

		// Delivers every queued event, including those published while delivering, then leaves
		// the queue empty. Not re-entrant: a handler must not call it.
		void Deliver(World& world);

		// Events published and not yet delivered.
		[[nodiscard]] std::size_t PendingCount() const noexcept { return queue.size(); }

		template <Event T> [[nodiscard]] std::size_t SubscriberCount() const
		{
			const Channel<T>* channel = FindChannel<T>();
			return channel == nullptr ? 0 : channel->handlers.size();
		}
	};
} // namespace engine::ecs
