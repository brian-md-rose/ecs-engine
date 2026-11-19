# Gamepads

Up to four pads (`input::kMaxPads`) are read into `Input::pads`, next to the keyboard and mouse. Use them through the
`Input` helpers (`PadDown`, `PadPressed`, `PadReleased`, `PadPushed`, `PadDirectionHeld`, `PadRightStick`), which look at
the first connected pad, or through `input.pads[i]` for a particular one.

## Buttons and names

Buttons are named by position, not by the label printed on them: `South`, `East`, `West`, `North`, `Start`, `Back`, `L1`,
`R1`, `LStick`, `RStick` and the four d-pad directions. `South` is A on an Xbox pad, Cross on a PlayStation pad and B on
a Switch pad. `Input::PadFamilyInUse()` gives the `PadFamily` (`Xbox`, `PlayStation`, `Switch`, `SteamDeck`, `Generic`),
which comes from SDL's gamepad type, plus a Steam Deck check by vendor and product id (0x28de, 0x1205) or name. Turn a
button into text for a hint with `PadButtonLabel(family, button)`.

## Sticks and triggers

Sticks are y-down like the screen (up is negative y). A circular deadzone (`kDefaultDeadzone`, 0.2, per pad) is applied
and the rest rescaled to 0..1, so a stick resting off centre reads zero. `StickHeld` and `StickWasPushed` treat a stick
as four directions with a push threshold of 0.5; `DirectionHeld` and `DirectionWasPushed` combine the d-pad and the left
stick, so game code asks for a direction once.

## Edges

`WasPressed` and `WasReleased` are latched: a button that went down and up between two updates still reports a press.
Unplugging a pad releases whatever it held.

## Hotplug and `lastUsed`

`Application` opens a pad when SDL reports `SDL_EVENT_GAMEPAD_ADDED` and puts it in the first free slot; removal frees
the slot. `Input::lastUsed` is `Device::Keyboard`, `Mouse` or `Pad` and changes on real input only: a key, a click or
move, a pad button, or a stick past its deadzone. Use it to switch button icons and hide the mouse cursor while a pad is
in use.

## UI focus

`UiFocusSystem` also reads the pad. The d-pad or left stick moves `Focused` to the nearest `FocusOrder` entity in that
direction (`ui::PickFocusNeighbour`: it must lie ahead of the current one, and sideways offset counts double; there is no
wrapping). If nothing has focus, the first push focuses the first stop. `South` publishes `Clicked` for a focused,
enabled `Interactable`, as a mouse click would. The right stick scrolls a `ScrollView` that has `Focused`, at
`ScrollView::padSpeed` units a second.

## In Tangle Run

`Controls` binds pad buttons like keys (`BindPad`, `UnbindPad`, `GetPadBinding`): South confirms, Start and Back pause,
and the d-pad and left stick move. The UI demo screen is fully navigable by pad and shows hints for the pad family in use.
