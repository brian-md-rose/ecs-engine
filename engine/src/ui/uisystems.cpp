#include "ui/uisystems.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "graphics/sprite.hpp"
#include "input/input.hpp"
#include "ui/ninepatch.hpp"
#include "ui/ui.hpp"

namespace engine::ui
{
	namespace
	{
		[[nodiscard]] graphics::Camera CameraOf(const ecs::World& world)
		{
			const UiCamera* camera = world.resources.TryGet<UiCamera>();
			return camera == nullptr ? graphics::Camera{} : camera->camera;
		}

		struct Candidate
		{
			ecs::Entity entity;
			std::uint8_t layer = 0;
			bool blocks = true;
		};

		// Removes tag T from every entity that has it and isn't keep.
		template <typename T> void ClearTagExcept(ecs::Registry& registry, ecs::Entity keep)
		{
			std::vector<ecs::Entity> stale;
			registry.Each<T>([&](ecs::Entity entity, T&)
			{
				if (entity != keep) stale.push_back(entity);
			});
			for (const ecs::Entity entity : stale)
				registry.Remove<T>(entity);
		}

	} // namespace

	void UiPointerSystem(ecs::World& world, const ecs::FrameContext& context)
	{
		PointerState* pointer = world.resources.TryGet<PointerState>();
		if (pointer == nullptr || context.input == nullptr) return;
		ecs::Registry& registry = world.registry;
		const input::Mouse& mouse = context.input->mouse;
		const graphics::Camera camera = CameraOf(world);

		// Everything under the pointer, bottom to top: by layer, then by registry order.
		std::vector<Candidate> hits;
		registry.Each<graphics::Transform, Hitbox>(
		    [&](ecs::Entity entity, graphics::Transform& transform, Hitbox& hitbox)
		{
			if (!PointerHits(registry, entity, transform, hitbox, mouse, camera)) return;
			hits.push_back({.entity = entity, .layer = hitbox.layer, .blocks = hitbox.blocksBelow});
		});
		std::ranges::stable_sort(hits, [](const Candidate& a, const Candidate& b)
		{
			return a.layer < b.layer;
		});

		// From the top down: the first interactable is the target, and the first blocker hides the rest.
		ecs::Entity target = ecs::Entity::Null;
		bool blocked = false;
		for (std::size_t i = hits.size(); i-- > 0;)
		{
			const Candidate& hit = hits[i];
			if (target.IsNull() && registry.HasComponent<Interactable>(hit.entity)) target = hit.entity;
			if (hit.blocks)
			{
				blocked = true;
				break;
			}
		}
		pointer->overUi = blocked || !target.IsNull();
		// A disabled button is still in the way, but nothing happens to it.
		ecs::Entity active = target;
		if (!active.IsNull() && !registry.GetComponent<Interactable>(active).enabled) active = ecs::Entity::Null;

		pointer->hovered = active;
		ClearTagExcept<Hovered>(registry, active);
		if (!active.IsNull() && !registry.HasComponent<Hovered>(active)) registry.Add<Hovered>(active);

		bool consumed = false;
		for (std::size_t b = 0; b < input::kMouseButtonCount; ++b)
		{
			const auto button = static_cast<input::MouseButton>(b);
			if (mouse.WasPressed(button))
			{
				pointer->pressed[b] = active;
				pointer->pressedOverUi[b] = pointer->overUi;
				if (!active.IsNull() && !registry.HasComponent<Pressed>(active)) registry.Add<Pressed>(active);
				if (pointer->overUi) consumed = true;
			}
			if (mouse.WasReleased(button))
			{
				const ecs::Entity pressedOn = pointer->pressed[b];
				if (!pressedOn.IsNull() && pressedOn == active && registry.IsAlive(pressedOn))
				{
					world.events.Publish(Clicked{.entity = pressedOn, .button = button});
				}
				if (pointer->pressedOverUi[b]) consumed = true;
				pointer->pressed[b] = ecs::Entity::Null;
				pointer->pressedOverUi[b] = false;
			}
		}
		pointer->clickConsumed = consumed;

		// Pressed stays while any button that went down on the entity is held.
		std::vector<ecs::Entity> stale;
		registry.Each<Pressed>([&](ecs::Entity entity, Pressed&)
		{
			if (std::ranges::find(pointer->pressed, entity) == pointer->pressed.end()) stale.push_back(entity);
		});
		for (const ecs::Entity entity : stale)
			registry.Remove<Pressed>(entity);
	}

	void UiFocusSystem(ecs::World& world, const ecs::FrameContext& context)
	{
		if (context.input == nullptr || !context.input->keyboard.WasPressed(input::Key::Tab)) return;
		ecs::Registry& registry = world.registry;

		struct Stop
		{
			ecs::Entity entity;
			int order = 0;
		};
		std::vector<Stop> stops;
		registry.Each<FocusOrder>([&](ecs::Entity entity, FocusOrder& focusOrder)
		{
			const Interactable* interactable = registry.TryGetComponent<Interactable>(entity);
			if (interactable != nullptr && !interactable->enabled) return;
			stops.push_back({.entity = entity, .order = focusOrder.order});
		});
		std::ranges::stable_sort(stops, [](const Stop& a, const Stop& b)
		{
			return a.order < b.order;
		});

		std::vector<ecs::Entity> current;
		registry.Each<Focused>([&](ecs::Entity entity, Focused&)
		{
			current.push_back(entity);
		});
		for (const ecs::Entity entity : current)
			registry.Remove<Focused>(entity);
		if (stops.empty()) return;

		const bool backwards = context.input->keyboard.IsDown(input::Key::Shift);
		const auto count = static_cast<std::ptrdiff_t>(stops.size());
		std::ptrdiff_t index = backwards ? count - 1 : 0; // nothing focused yet: start at the end it came from
		for (std::ptrdiff_t i = 0; i < count && !current.empty(); ++i)
		{
			if (stops[static_cast<std::size_t>(i)].entity != current.front()) continue;
			index = (i + (backwards ? count - 1 : 1)) % count;
			break;
		}
		registry.Add<Focused>(stops[static_cast<std::size_t>(index)].entity);
	}

	void ScrollViewSystem(ecs::World& world, const ecs::FrameContext& context)
	{
		if (context.input == nullptr) return;
		ecs::Registry& registry = world.registry;
		const input::Mouse& mouse = context.input->mouse;
		const graphics::Camera camera = CameraOf(world);

		registry.Each<graphics::Transform, Hitbox, ScrollView>(
		    [&](ecs::Entity entity, graphics::Transform& transform, Hitbox& hitbox, ScrollView& view)
		{
			const bool over = PointerHits(registry, entity, transform, hitbox, mouse, camera);
			if (over && mouse.wheel != 0.0f) view.offset -= mouse.wheel * view.wheelStep;

			// Drag: the content follows the pointer once it has moved past the threshold.
			const float pointerY = PointerInSpaceOf(registry, entity, mouse, camera).position.y;
			if (over && mouse.WasPressed(input::MouseButton::Left))
			{
				view.dragging = true;
				view.dragMoved = false;
				view.dragGrabY = pointerY;
				view.dragGrabOffset = view.offset;
			}
			if (view.dragging)
			{
				if (!mouse.IsDown(input::MouseButton::Left))
				{
					view.dragging = false;
				}
				else
				{
					const float moved = pointerY - view.dragGrabY;
					if (std::fabs(moved) > view.dragThreshold) view.dragMoved = true;
					if (view.dragMoved) view.offset = view.dragGrabOffset - moved;
				}
			}

			view.offset = ClampScroll(view.offset, view.contentHeight, view.viewHeight);
			if (graphics::LocalTransform* local = registry.TryGetComponent<graphics::LocalTransform>(view.content))
			{
				local->position.y = -view.offset;
			}
		});
	}

	void NinePatchSystem(ecs::World& world, const ecs::FrameContext&)
	{
		ecs::Registry& registry = world.registry;
		std::vector<ecs::Entity> patches;
		registry.Each<NinePatch>([&](ecs::Entity entity, NinePatch&)
		{
			patches.push_back(entity);
		});

		for (const ecs::Entity entity : patches)
		{
			if (!registry.HasComponent<NinePatchPieces>(entity))
			{
				// Built in a local first: Create and Add must not run while a reference into a pool is held.
				NinePatchPieces made;
				for (ecs::Entity& piece : made.pieces)
				{
					piece = registry.Create();
					registry.Add<ecs::Parent>(piece, ecs::Parent{.entity = entity});
					registry.Add<graphics::Transform>(piece);
					registry.Add<graphics::LocalTransform>(piece);
				}
				registry.Add<NinePatchPieces>(entity, made);
			}

			const NinePatch patch = registry.GetComponent<NinePatch>(entity);
			const NinePatchPieces pieces = registry.GetComponent<NinePatchPieces>(entity);
			const std::array<NinePatchPiece, 9> layout = NinePatchLayout(patch);
			for (std::size_t i = 0; i < pieces.pieces.size(); ++i)
			{
				const ecs::Entity piece = pieces.pieces[i];
				if (!registry.IsAlive(piece)) continue;
				registry.GetComponent<graphics::LocalTransform>(piece).position = layout[i].offset;
				if (!layout[i].Visible())
				{
					if (registry.HasComponent<graphics::Sprite>(piece)) registry.Remove<graphics::Sprite>(piece);
					continue;
				}
				if (!registry.HasComponent<graphics::Sprite>(piece)) registry.Add<graphics::Sprite>(piece);
				graphics::Sprite& sprite = registry.GetComponent<graphics::Sprite>(piece);
				sprite.source = layout[i].source;
				sprite.width = layout[i].width;
				sprite.height = layout[i].height;
				sprite.tint = patch.tint;
				sprite.layer = patch.layer;
			}
		}
	}
} // namespace engine::ui
