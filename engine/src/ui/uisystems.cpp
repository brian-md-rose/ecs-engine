#include "ui/uisystems.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "ecs/entity.hpp"
#include "ecs/hierarchy.hpp"
#include "graphics/sprite.hpp"
#include "input/input.hpp"
#include "ui/focusnav.hpp"
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
		if (context.input == nullptr) return;
		const input::Input& in = *context.input;
		const bool tab = in.keyboard.WasPressed(input::Key::Tab);
		std::optional<input::PadDirection> pushed;
		for (const input::PadDirection direction : {input::PadDirection::Up, input::PadDirection::Down,
		                                            input::PadDirection::Left, input::PadDirection::Right})
		{
			if (!pushed && in.PadPushed(direction)) pushed = direction;
		}
		const bool confirm = in.PadPressed(input::PadButton::South);
		if (!tab && !pushed && !confirm) return;
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

		// South presses the focused thing, as a click.
		if (confirm && !current.empty() && registry.IsAlive(current.front()) &&
		    registry.TryGetComponent<Interactable>(current.front()) != nullptr &&
		    registry.TryGetComponent<Interactable>(current.front())->enabled)
		{
			world.events.Publish(Clicked{.entity = current.front(), .button = input::MouseButton::Left});
		}
		if (!tab && !pushed) return;

		if (stops.empty())
		{
			for (const ecs::Entity entity : current)
				registry.Remove<Focused>(entity);
			return;
		}
		const auto count = static_cast<std::ptrdiff_t>(stops.size());
		std::ptrdiff_t index = 0;
		const auto position = [&]() -> std::optional<std::ptrdiff_t>
		{
			for (std::ptrdiff_t i = 0; i < count && !current.empty(); ++i)
			{
				if (stops[static_cast<std::size_t>(i)].entity == current.front()) return i;
			}
			return std::nullopt;
		}();

		if (tab)
		{
			const bool backwards = in.keyboard.IsDown(input::Key::Shift);
			index = position ? (*position + (backwards ? count - 1 : 1)) % count : (backwards ? count - 1 : 0);
		}
		else
		{
			const auto centreOf = [&](ecs::Entity entity)
			{
				graphics::Position centre{};
				if (const auto* transform = registry.TryGetComponent<graphics::Transform>(entity))
				{
					centre = transform->position;
					if (const Hitbox* hitbox = registry.TryGetComponent<Hitbox>(entity))
					{
						centre.x += hitbox->width / 2.0f;
						centre.y += hitbox->height / 2.0f;
					}
				}
				return centre;
			};
			std::vector<FocusCandidate> candidates;
			for (const Stop& stop : stops)
			{
				candidates.push_back({.entity = stop.entity, .centre = centreOf(stop.entity)});
			}
			// The origin is the focused stop, or, if that has since been disabled, where it still is, so focus
			// moves on from there rather than jumping to the first stop.
			std::optional<std::size_t> from;
			if (position)
			{
				from = static_cast<std::size_t>(*position);
			}
			else if (!current.empty() && registry.IsAlive(current.front()) &&
			         registry.TryGetComponent<graphics::Transform>(current.front()) != nullptr)
			{
				candidates.push_back({.entity = current.front(), .centre = centreOf(current.front())});
				from = candidates.size() - 1;
			}
			// Toward the nearest stop in the direction pushed; stay put if there is none that way. With nothing
			// focused yet, the first stop gets it.
			if (from)
			{
				const std::optional<std::size_t> next = PickFocusNeighbour(candidates, *from, *pushed);
				if (next)
					index = static_cast<std::ptrdiff_t>(*next);
				else if (position)
					index = *position;
				else
					return; // a disabled focus with nowhere to go keeps its tag
			}
		}

		for (const ecs::Entity entity : current)
			registry.Remove<Focused>(entity);
		registry.Add<Focused>(stops[static_cast<std::size_t>(index)].entity);
	}

	void ScrollViewSystem(ecs::World& world, const ecs::FrameContext& context)
	{
		if (context.input == nullptr) return;
		ecs::Registry& registry = world.registry;
		const input::Mouse& mouse = context.input->mouse;
		const graphics::Camera camera = CameraOf(world);
		const float padPush = context.input->PadRightStick().y; // positive is down the list

		registry.Each<graphics::Transform, Hitbox, ScrollView>(
		    [&](ecs::Entity entity, graphics::Transform& transform, Hitbox& hitbox, ScrollView& view)
		{
			const bool over = PointerHits(registry, entity, transform, hitbox, mouse, camera);
			if (over && mouse.wheel != 0.0f) view.offset -= mouse.wheel * view.wheelStep;
			// A pad's right stick scrolls the view that has focus, at a speed that follows how far it is pushed.
			if (padPush != 0.0f && registry.TryGetComponent<Focused>(entity) != nullptr)
			{
				view.offset += padPush * view.padSpeed * static_cast<float>(context.dt);
			}

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

		// Pieces whose panel was destroyed would stay drawn where they were.
		std::vector<ecs::Entity> orphans;
		registry.Each<NinePatchChild, ecs::Parent>([&](ecs::Entity entity, NinePatchChild&, ecs::Parent& parent)
		{
			if (!registry.IsAlive(parent.entity)) orphans.push_back(entity);
		});
		for (const ecs::Entity entity : orphans)
			registry.Destroy(entity);

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
					registry.Add<NinePatchChild>(piece);
					registry.Add<graphics::Transform>(piece);
					registry.Add<graphics::LocalTransform>(piece);
				}
				registry.Add<NinePatchPieces>(entity, made);
			}

			const NinePatch patch = registry.GetComponent<NinePatch>(entity);
			const NinePatchPieces pieces = registry.GetComponent<NinePatchPieces>(entity);
			std::array<NinePatchPiece, 9> layout{};
			NinePatchLayout(patch, layout);
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
