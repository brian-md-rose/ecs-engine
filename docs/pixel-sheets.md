# Pixel sheets

The game's pictures (the font in `font.txt`, and every sprite in `sprites.txt`) are drawn by
hand as text, in `game/assets/sprites/`. Text keeps them small, readable in a diff, editable in
any editor, and loadable without an image library. The engine parses them
(`graphics/pixelsheet.hpp`), packs every picture into one atlas (`graphics/atlas.hpp`) and draws
them from one texture.

## Format

```text
# A comment. Blank lines and comments may go between entries.
color X 255 210 0        character, red, green, blue, and an optional alpha (0-255, default 255)
color s 40 40 60

sprite blink 5 3 2       name, frame width, frame height, and an optional frame count (default 1)
.XXX...X..               then frame-height rows, each frames * frame-width characters wide
XsXsX.XsX.
.XXX...X..
```

- `.` is always transparent. Every other character in a row must have been given a colour
  with `color` earlier in the file; a colour is defined once.
- A sprite's frames sit side by side, frame 0 on the left, so an animation reads like a strip
  of film. They become one image `frames * width` pixels wide, so frame `i` is `i * width`
  pixels to the right of frame 0 in the atlas.
- Sprite names are unique within a file. Rows are read exactly, so they may contain `#`.
- Errors give the file, line and column, the way compilers do: `font.txt:12:4: no colour is
  defined for 'Q'`.

## The font

`font.txt` holds one 5 x 7 sprite per character, named after it (`sprite A 5 7`). The game
draws them 6 pixels apart on lines 8 pixels apart, at the logical resolution, so they stay
sharp at every window scale. Glyphs are white and take the colour of the `Text` drawing them.
The font has capitals, digits and `! - : . / < >`; `@` is the life icon.

## The sprites

`sprites.txt` holds every other picture, all 8 x 8 (one maze tile): the player's walk in three
directions (left is right, mirrored) and its death; Grit, Snare, Clasp and Wisp; the frightened
look, its warning flash and the eaten "returning" look; the pellet, the pulsing power pellet
and the door. How fast each animation plays is set in `game/src/assets.cpp`.

Walls are not drawn one by one. `sprites.txt` has a wall core and an edge for each side, and the
game builds the sixteen wall tiles from them at start-up (`walltiles.hpp`): each wall shows an
edge only on the sides that don't face another wall, so the maze's walls join up into outlined
blocks.
