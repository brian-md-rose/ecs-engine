# UI

The engine's UI is a set of mechanisms, not a toolkit: no widget has a look or a layout of its own. A game builds its
screens from sprites, text and the components below, and decides what a click does. Everything is an ordinary
component or system, so it works with states, events and `DespawnOnExit` like the rest of the engine.

Tangle Run has a working example: OPTIONS, then UI DEMO (`game/src/uidemo.cpp`). It has a nine-slice panel, three
buttons, a clipped list of 30 wrapped-text rows, and a cursor sprite, all driven by the mouse.

## Setup

```cpp
world.resources.Emplace<ui::PointerState>();                      // and ui::UiCamera if the UI is in the world
scheduler.Add(Phase::Input,      "UiPointer",  ui::UiPointerSystem);
scheduler.Add(Phase::Input,      "UiFocus",    ui::UiFocusSystem);
scheduler.Add(Phase::Input,      "ScrollView", ui::ScrollViewSystem);
scheduler.Add(Phase::LateUpdate, "NinePatch",  ui::NinePatchSystem);
world.events.Subscribe<ui::Clicked>(OnClick);                      // what clicks do
```

Keep the order. Systems that act on clicks run after the Input phase, so they see this update's `PointerState`
(events are delivered at the end of the phase). The systems may carry a run condition: Tangle Run runs the first
three only on the demo screen. `NinePatchSystem` is cheap and also tidies up after a screen (see below), so let it run
all the time.

The mouse itself is `input.mouse` (`input/mouse.hpp`): its position in logical pixels (so letterboxing and window
scaling are already taken out), the three buttons with `IsDown`, `WasPressed` and `WasReleased`, the wheel in notches
(positive away from the user, whatever the operating system's "natural scrolling" says), and `inside`, false when
the pointer is outside the window. A game that draws its own pointer returns `false` from `Game::ShowSystemCursor()`
to hide the system's.

## Screen space and the camera

An entity is normally in the world: its `Transform` is in world units and the `Camera` (position, zoom, offset)
decides where it appears. Add the `ScreenSpace` tag and its `Transform` is in logical screen pixels instead, so
the camera does not touch it. Children inherit the tag through `ecs::Parent`, so a panel needs one tag, not one per
piece.

```cpp
registry.Add<ScreenSpace>(panel);               // HUD panels, menus, cursors
```

`Camera::WorldToScreen`, `ScreenToWorld` and `ViewSize` convert between the two. Hit-testing uses the same rules as
drawing: a screen-space `Hitbox` is hit at the pointer's screen position, a world one at the world point under it
(through `UiCamera`, which you keep equal to `Renderer2D::camera`).

## Clipping

`ClipRect{rect}` confines an entity, and everything under it through `ecs::Parent`, to a rectangle of logical screen
pixels. It ignores the camera, and nested clips narrow to where they overlap. Things wholly outside are not drawn.

The draw order does not change: sprites, shapes and text still sort by `layer`. The batch splits where the clip
changes (`DrawBatch::Runs()`), and `Renderer2D` sets SDL's clip rectangle once per run, so a list costs one extra
draw call, not one per row.

Clipping also decides what the pointer can hit: the part of a hitbox outside its clip does not count, so a row
scrolled out of view cannot be clicked.

## Hit-testing

`Hitbox{width, height, layer, blocksBelow}` is a rectangle with its top-left at the entity's `Transform`, in the
entity's own units. The pointer is over the hitbox with the highest `layer`; on a tie, the one later in the registry.
Give it the layer the thing is drawn on, so what you see on top is what you hit.

- A hitbox **blocks** what is below it (`blocksBelow`, true by default). Put one on a plain panel so clicks do not
  fall through it to the world. Set it false for something the pointer should pass through.
- Add `Interactable{enabled}` to make it a button. The system then sets the tags `Hovered` (the pointer is over it) and
  `Pressed` (a button went down on it and is still held), and publishes `Clicked{entity, button}` when a button
  goes down and comes up on the same enabled entity. Press on one button, drag off and release elsewhere, and
  nothing is clicked. A disabled one still blocks, but gets no tags and no clicks.
- Style a button by reading the tags. The demo's `UiDemoLookSystem` picks the `button`, `button_hover` or
  `button_pressed` picture from `Hovered` and `Pressed`.
- A label drawn on a button should have no `Hitbox`, or one with `blocksBelow = false`.

`ui::PointerHits` and `PointerInSpaceOf` are the pure parts, if you need to test a point yourself.

## Consuming clicks

A click on a button must not also act on the world beneath it. `PointerState` is the answer, as a resource:

| Field | Meaning |
|-------|---------|
| `overUi` | the pointer is over a hitbox that blocks, or an interactable one |
| `clickConsumed` | a button went down over the UI this update, or came up after going down over it |
| `hovered` | the interactable entity under the pointer, or `Null` |
| `pressed[b]`, `pressedOverUi[b]` | the entity each button went down on, while held, and whether that was over the UI |

A system that reacts to the world (placing a tower, say) checks `!pointer.clickConsumed` first. It does not need to know
what the UI is.

```cpp
const auto& pointer = world.resources.Get<ui::PointerState>();
if (context.input->mouse.WasPressed(MouseButton::Left) && !pointer.clickConsumed) PlaceTower(world);
```

## Nine-slice panels

`NinePatch{source, left, right, top, bottom, width, height, tint, layer}` draws a panel, button or bar frame from one
small picture. The border widths stay as drawn and the edges and middle stretch, so one 9 by 9 picture makes a panel of
any size. `NinePatchSystem` creates nine child sprites (tagged `NinePatchChild`) and keeps them sized. `ScreenSpace`
and `ClipRect` on the entity apply to the pieces. If the panel is smaller than its borders the borders shrink to fit.

Destroying the entity is enough: the next run of the system destroys pieces left without a parent. That matters for
`DespawnOnExit`, which destroys only the entity that carries it. (`ecs::DestroyWithChildren` removes them at once.)
`NinePatchLayout` is the pure arithmetic.

## Scrolling lists

```cpp
const Entity view = registry.Create();                  // the window onto the list
registry.Add<Transform>(view, at);
registry.Add<ScreenSpace>(view);
registry.Add<ClipRect>(view, ClipRect{.rect = {x, y, width, height}});
registry.Add<Hitbox>(view, Hitbox{.width = w, .height = h, .layer = layer});

const Entity content = registry.Create();               // the rows' parent, moved by the system
registry.Add<Transform>(content, at);
registry.Add<Parent>(content, Parent{view});
registry.Add<LocalTransform>(content);

registry.Add<ScrollView>(view, ScrollView{.content = content, .viewHeight = h, .contentHeight = total});
// each row: Parent{content}, LocalTransform{{x, y}}, Transform, and a Text or Sprite
```

The wheel over the view, or dragging it with the left button, moves the content; `ScrollViewSystem` keeps the content's
`LocalTransform` y at `-offset`, with `offset` clamped to `0` through `contentHeight - viewHeight` (`ClampScroll`), so a
list that fits does not move. A drag shorter than `dragThreshold` pixels does not scroll, so a click on a row stays a
click. The view tests only its own `Hitbox`, so something drawn over it (a modal panel) will not stop it scrolling:
skip the scrolling while one is open.

## Wrapped text

`Text::wrapWidth` (pixels, 0 for none) breaks the text at spaces to fit, splitting a word only if it is wider than the
line; `align` places each line against the entity's x (left, centre or right). `MeasureText(font, text, maxWidth,
scale)` gives the size the text will take, which is how the demo works out each row's height and so the list's
`contentHeight`. `WrapText` returns the lines. Characters the font does not have draw as its placeholder, or `?`.

## Keyboard focus

Give things a `FocusOrder{order}` and Tab moves the `Focused` tag to the next, Shift+Tab to the previous, wrapping at
the ends. An `Interactable` is reached only while it is enabled. The system sets the tag and nothing else: draw a focus
frame by reading `Focused`, and decide what Enter does on a focused entity.

## Testing

The pointer is just input, so a test drives it with `input.mouse.OnMove`, `OnButton` and `OnWheel` and then runs updates,
as `tests/game/uidemo_tests.cpp` does: click at a button's centre and check the counter, wheel over the list and check
`ScrollView::offset`, leave the screen and check nothing is left over.
