# Maze format

Mazes are plain text files in `game/assets/mazes/`, one character per tile and one line per
row of tiles. A text editor is the level editor: what you see in the file is the maze.

## Tiles

| Character | Tile              | Meaning                                                       |
|-----------|-------------------|---------------------------------------------------------------|
| `#`       | Wall              | Blocks movement.                                              |
| `.`       | Pellet            | Floor with a pellet on it.                                    |
| `o`       | Power pellet      | Floor with a power pellet on it.                              |
| ` `       | Empty (space)     | Floor with nothing on it.                                     |
| `P`       | Player spawn      | Empty floor where the player starts.                          |
| `E`       | Enemy spawn       | Empty floor where an enemy starts. Usually inside the house.  |
| `-`       | Enemy-house door  | Blocks the player; enemies pass through it.                   |
| `T`       | Wrap tunnel       | Empty floor on the edge; leaving through it enters at its pair. |

Characters are case-sensitive. Anything else is an error.

Coordinates are counted from the top-left tile, which is column 0, row 0. The file says nothing
about pixels: the game decides how big a tile is (8 logical pixels for now).

## Rules

The loader checks these and reports the first one broken (see [Errors](#errors)).

1. **Rectangular.** Every row has the same number of tiles as the first. Trailing spaces count,
   so a row must not end in a space: close every row with a wall or a tunnel. (Editors trim
   trailing spaces, so a row that ends in one would silently get shorter.)
2. **Not empty.** At least one row.
3. **Exactly one `P`.**
4. **At least one `E`.**
5. **Tunnels come in pairs on opposite edges.** A `T` on the left edge needs a `T` on the right
   edge of the same row, and a `T` on the top edge needs one on the bottom edge of the same
   column. A `T` that is not on an edge, or is in a corner, is an error.

Line endings may be LF or CRLF, and blank lines at the end of the file are ignored. A blank line
anywhere else is a row with no tiles, so it breaks rule 1.

What the loader does not check, and is up to whoever designs the maze: that every floor tile can
be reached, that there are no dead ends, and that the enemy house is closed except for its door.

## Example

A small maze with a house in the middle, its door below it, and a tunnel through the middle row:

```text
#############
#o....#....o#
#.##.###.##.#
#....#E#....#
T.##.#-#.##.T
#.....P.....#
#o#.#####.#o#
#...........#
#############
```

## Errors

Errors give the file, the line and the column, both counted from 1, in the form compilers use, so
editors and terminals can jump to them:

```text
game/assets/mazes/level1.txt:4:7: unknown tile 'x'
game/assets/mazes/level1.txt:9:13: row 9 has 12 tiles; expected 13 like row 1
game/assets/mazes/level1.txt:5:13: tunnel on the right edge has no pair on the left edge
game/assets/mazes/level1.txt: no player spawn 'P'
```

Errors about the maze as a whole (a missing spawn) have no line or column. A second `P` is
reported at the second one.
