#include "debug/debugpanel.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/font.hpp"
#include "graphics/renderer2d.hpp"
#include "graphics/shape.hpp"
#include "graphics/transform.hpp"

namespace engine::debug
{
	namespace
	{
		using ecs::Entity;
		using ecs::Registry;
		using ecs::Scheduler;
		using ecs::World;

		void AppendUpper(std::string& text, std::string_view part)
		{
			for (const char c : part)
			{
				text += (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
			}
		}

		// left, then right against the end of a width-character cell; left is cut short if both
		// don't fit with a space between them.
		void AppendCell(std::string& text, std::string_view left, std::string_view right, std::size_t width)
		{
			const std::size_t room = width > right.size() + 1 ? width - right.size() - 1 : 0;
			const std::string_view shown = left.substr(0, std::min(left.size(), room));
			AppendUpper(text, shown);
			text.append(width > shown.size() + right.size() ? width - shown.size() - right.size() : 1, ' ');
			text += right;
		}

		// value with decimals digits after the point.
		[[nodiscard]] std::string Fixed(double value, int decimals)
		{
			char buffer[32];
			const int length = std::snprintf(buffer, sizeof buffer, "%.*f", decimals, value);
			return std::string(buffer,
			                   static_cast<std::size_t>(std::clamp(length, 0, static_cast<int>(sizeof buffer) - 1)));
		}
	} // namespace

	std::string DebugPanelText(const DebugPanelView& view, std::size_t maxSystems, std::size_t columns)
	{
		std::string text;

		if (view.frames != nullptr && view.frames->HasReport())
		{
			text += "FPS " + Fixed(view.frames->Fps(), 0) + "  DRAW " + Fixed(view.frames->AverageDrawMs(), 2) +
			        " MS  " + std::to_string(view.frames->Items()) + " ITEMS";
		}
		else
		{
			text += "FPS --";
		}
		text += '\n';
		text += "ENTITIES " + std::to_string(view.entities);
		text += "\n\n";

		// The slowest systems first; skipped ones after every system that ran.
		std::vector<Scheduler::SystemTiming> systems(view.systems.begin(), view.systems.end());
		std::ranges::stable_sort(systems, [](const Scheduler::SystemTiming& a, const Scheduler::SystemTiming& b)
		{
			if (a.skipped != b.skipped) return !a.skipped;
			return a.averageMs > b.averageMs;
		});
		AppendCell(text, "SYSTEMS " + std::to_string(systems.size()), "MS", columns);
		text += '\n';
		for (std::size_t i = 0; i < std::min(maxSystems, systems.size()); ++i)
		{
			const Scheduler::SystemTiming& system = systems[i];
			AppendCell(text, system.name, system.skipped ? "OFF" : Fixed(system.averageMs, 3), columns);
			text += '\n';
		}

		text += '\n';
		AppendUpper(text, "COMPONENTS " + std::to_string(view.components.size()));
		const std::size_t half = columns / 2;
		for (std::size_t i = 0; i < view.components.size(); ++i)
		{
			text += i % 2 == 0 ? '\n' : ' ';
			const Registry::ComponentCount& component = view.components[i];
			AppendCell(text, component.name, std::to_string(component.count), i % 2 == 0 ? half - 1 : columns - half);
		}
		return text;
	}

	void AddDebugPanel(World& world, Scheduler& scheduler)
	{
		world.resources.Emplace<DebugPanel>();
		if (!world.resources.Has<core::FrameStats>()) world.resources.Emplace<core::FrameStats>();
		scheduler.Add(ecs::Phase::Input, "DebugPanelToggle", DebugPanelToggleSystem);
		scheduler.Add(ecs::Phase::Render, "DebugPanel", DebugPanelSystem);
	}

	void DebugPanelToggleSystem(World& world, const ecs::FrameContext& context)
	{
		if (context.keyboard == nullptr) return;
		DebugPanel& panel = world.resources.Get<DebugPanel>();
		if (!context.keyboard->WasPressed(panel.toggleKey)) return;

		panel.visible = !panel.visible;
		if (panel.visible)
		{
			panel.background = world.registry.Create();
			world.commands.Add<graphics::Transform>(panel.background);
			world.commands.Add<graphics::Shape>(
			    panel.background, graphics::Shape{.color = kDebugPanelBackground, .layer = kDebugPanelLayer});
			panel.text = world.registry.Create();
			world.commands.Add<graphics::Transform>(panel.text);
			world.commands.Add<graphics::Text>(
			    panel.text, graphics::Text{.color = kDebugPanelTextColor, .layer = kDebugPanelLayer});
		}
		else
		{
			world.commands.Destroy(panel.background);
			world.commands.Destroy(panel.text);
			panel.background = ecs::Entity::Null;
			panel.text = ecs::Entity::Null;
		}
	}

	void DebugPanelSystem(World& world, const ecs::FrameContext& context)
	{
		const graphics::Renderer2D* renderer2D = world.resources.TryGet<graphics::Renderer2D>();
		core::FrameStats& frames = world.resources.Get<core::FrameStats>();
		if (renderer2D != nullptr)
		{
			frames.AddFrame(renderer2D->LastDrawTime(), renderer2D->ItemCount());
		}
		else
		{
			frames.AddFrame({}, 0);
		}

		const DebugPanel& panel = world.resources.Get<DebugPanel>();
		Registry& registry = world.registry;
		if (!panel.visible || renderer2D == nullptr || renderer2D->GetFont() == nullptr) return;
		if (!registry.IsAlive(panel.text) || !registry.HasComponent<graphics::Text>(panel.text)) return;

		const graphics::BitmapFont& font = *renderer2D->GetFont();
		const graphics::Camera& camera = renderer2D->camera;
		const auto usable = static_cast<float>(camera.logicalWidth) - 2.0f * kDebugPanelMargin;
		const auto columns =
		    static_cast<std::size_t>(std::max(usable, 0.0f)) / static_cast<std::size_t>(std::max(font.advance, 1));

		const std::vector<Registry::ComponentCount> components = registry.ComponentCounts();
		const std::vector<Scheduler::SystemTiming> systems =
		    context.scheduler != nullptr ? context.scheduler->Timings() : std::vector<Scheduler::SystemTiming>{};
		const DebugPanelView view{
		    .frames = &frames, .entities = registry.EntityCount(), .components = components, .systems = systems};

		graphics::Text& text = registry.GetComponent<graphics::Text>(panel.text);
		text.value = DebugPanelText(view, panel.maxSystems, columns);
		registry.GetComponent<graphics::Transform>(panel.text).position = {camera.position.x + kDebugPanelMargin,
		                                                                   camera.position.y + kDebugPanelMargin};

		if (registry.IsAlive(panel.background) && registry.HasComponent<graphics::Shape>(panel.background))
		{
			graphics::Shape& shape = registry.GetComponent<graphics::Shape>(panel.background);
			shape.width = static_cast<float>(graphics::TextWidth(font, text.value)) + 2.0f * kDebugPanelMargin;
			shape.height = static_cast<float>(graphics::TextHeight(font, text.value)) + 2.0f * kDebugPanelMargin;
			registry.GetComponent<graphics::Transform>(panel.background).position = camera.position;
		}
	}
} // namespace engine::debug
