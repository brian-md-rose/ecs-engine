#include "ecs/events.hpp"

namespace engine::ecs
{
	namespace
	{
		// Marks the bus as delivering and, on the way out, empties the queue, even if a handler
		// throws (as a failed assert does in tests): a half-delivered queue is dropped rather
		// than delivered twice.
		template <typename Clear> class DeliveryScope
		{
			bool& delivering;
			Clear clear;

		public:
			DeliveryScope(bool& flag, Clear c) : delivering(flag), clear(c) { delivering = true; }
			~DeliveryScope()
			{
				clear();
				delivering = false;
			}

			DeliveryScope(const DeliveryScope&) = delete;
			DeliveryScope& operator=(const DeliveryScope&) = delete;
			DeliveryScope(DeliveryScope&&) = delete;
			DeliveryScope& operator=(DeliveryScope&&) = delete;
		};
	} // namespace

	void EventBus::Deliver(World& world)
	{
		ENGINE_ASSERT(!delivering, "EventBus::Deliver is not re-entrant; a handler must not call it");
		const DeliveryScope scope(delivering, [this]() noexcept
		{
			for (IChannel* channel : queue)
			{
				channel->Clear();
			}
			queue.clear();
		});

		// By index: handlers may publish, which appends to queue while it is being walked.
		for (std::size_t i = 0; i < queue.size(); ++i)
		{
			ENGINE_VERIFY(i < kMaxEventsPerDelivery, "event handlers keep publishing events; check for a cycle");
			queue[i]->DeliverNext(world);
		}
	}
} // namespace engine::ecs
