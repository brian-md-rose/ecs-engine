#pragma once

// The one table of keyboard keys. Adding a key is a one-line change here: it becomes a Key
// enumerator, and the platform layer builds its SDL scancode map from the same line.
//
// KEY(Name, SDL) gives the engine's name for the key and the tail of its SDL scancode name
// (SDL_SCANCODE_<SDL>). Keys are physical positions (scancodes): WASD stays in the same place on
// AZERTY or Dvorak keyboards.
// clang-format off
#define ENGINE_KEY_LIST(KEY) \
	KEY(Up, UP) KEY(Down, DOWN) KEY(Left, LEFT) KEY(Right, RIGHT) \
	KEY(A, A) KEY(B, B) KEY(C, C) KEY(D, D) KEY(E, E) KEY(F, F) KEY(G, G) KEY(H, H) KEY(I, I) \
	KEY(J, J) KEY(K, K) KEY(L, L) KEY(M, M) KEY(N, N) KEY(O, O) KEY(P, P) KEY(Q, Q) KEY(R, R) \
	KEY(S, S) KEY(T, T) KEY(U, U) KEY(V, V) KEY(W, W) KEY(X, X) KEY(Y, Y) KEY(Z, Z) \
	KEY(Num0, 0) KEY(Num1, 1) KEY(Num2, 2) KEY(Num3, 3) KEY(Num4, 4) \
	KEY(Num5, 5) KEY(Num6, 6) KEY(Num7, 7) KEY(Num8, 8) KEY(Num9, 9) \
	KEY(F1, F1) KEY(F2, F2) KEY(F3, F3) KEY(F4, F4) KEY(F5, F5) KEY(F6, F6) \
	KEY(F7, F7) KEY(F8, F8) KEY(F9, F9) KEY(F10, F10) KEY(F11, F11) KEY(F12, F12) \
	KEY(Space, SPACE) KEY(Enter, RETURN) KEY(Escape, ESCAPE) KEY(Tab, TAB) \
	KEY(Backspace, BACKSPACE) KEY(Delete, DELETE) KEY(Home, HOME) KEY(End, END) \
	KEY(PageUp, PAGEUP) KEY(PageDown, PAGEDOWN) \
	KEY(Shift, LSHIFT) KEY(Ctrl, LCTRL) KEY(Alt, LALT)

// Second physical keys that count as a key above (the right-hand Shift, Ctrl and Alt). They add
// no enumerators. Known limit: the two
// physical keys share one state, so with both held, letting go of one reports the key as up.
#define ENGINE_KEY_ALIAS_LIST(KEY) KEY(Shift, RSHIFT) KEY(Ctrl, RCTRL) KEY(Alt, RALT)
// clang-format on
