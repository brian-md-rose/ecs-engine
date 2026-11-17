# Pixel sheets

The game's pictures (the font and every sprite) live in `game/assets/sprites/`. They can be
written two ways, and the engine turns both into the same `PixelSheet`
(`graphics/pixelsheet.hpp`), packs every picture into one atlas (`graphics/atlas.hpp`) and draws
them from one texture:

- **PNG sheets** (`font.sheet`, `sprites.sheet`): the art is PNG files, drawn in any image
  editor, and a small manifest names each sprite. This is what the game loads.
- **Text sheets** (`font.txt`, `sprites.txt`): the art is drawn as text, which is small and
  readable in a diff. The game falls back to these when a `.sheet` file is missing, so either
  one alone is enough.

For each of `font` and `sprites`, `LoadGameAssets` uses `name.sheet` if it exists, and
`name.txt` otherwise. `LoadSheetFile` (`graphics/sheetmanifest.hpp`) picks by extension.

## PNG sheets

```text
# A comment. Blank lines and comments may go between entries.
sprite player sprites/player.png 8 8 2   name, PNG file, frame width, frame height, frames (1)

image font.png                           the PNG that the regions below are cut from
region A 0 0 5 7                         name, x, y, frame width, frame height, frames (1)
```

- A `sprite` is a PNG of its own, `frames * frame width` by `frame height` pixels. Any other
  size is an error.
- A `region` is that rectangle of the latest `image`, so one PNG can hold many sprites. It
  must lie inside the image.
- PNG paths are relative to the manifest. Sprite names are unique within a file.
- Any PNG pixel format works; pixels keep their alpha (straight, not premultiplied).
- Errors give the file, line and column, like the text format.
- `font.sheet` is one `font.png` with a region per glyph; `sprites.sheet` has one PNG per sprite
  in `sprites/`.

`py tools/convert_sheet.py <file>.txt --each` (a PNG per sprite) or `--atlas <per row>` (one
PNG) turns a text sheet into PNGs and a manifest, which is how the current PNGs were made. A
game test checks that the PNG sheets and the text sheets hold identical pixels, so edit both
(or regenerate one from the other) when changing art.

## Text sheets

### Format

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

### The font

`font` holds one 5 x 7 sprite per character, named after it (`sprite A 5 7`). The game
draws them 6 pixels apart on lines 8 pixels apart, at the logical resolution, so they stay
sharp at every window scale. Glyphs are white and take the colour of the `Text` drawing them.
The font has capitals, digits and `! - : . / < >`; `@` is the life icon.

### The sprites

`sprites` holds every other picture, all 8 x 8 (one maze tile): the player's walk in three
directions (left is right, mirrored) and its death; Grit, Snare, Clasp and Wisp; the frightened
look, its warning flash and the eaten "returning" look; the pellet, the pulsing power pellet
and the door. How fast each animation plays is set in `game/src/assets.cpp`.

Walls are not drawn one by one. `sprites.txt` has a wall core and an edge for each side, and the
game builds the sixteen wall tiles from them at start-up (`walltiles.hpp`): each wall shows an
edge only on the sides that don't face another wall, so the maze's walls join up into outlined
blocks.
