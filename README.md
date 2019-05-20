# binpack

binpack is 2D bin packer implementing the MAXRECTS-BSSF algorithm
as outline in "A Thousand Ways to Pack the Bin - A Practical
Approach to Two-Dimensional Rectangle Bin Packing, Jukka Jylänki.

binpack includes a font-atlas based text renderer using HarfBuzz
and FreeType. Render time is less than 1 microsecond per glyph.

## rectangle-rectangle intersection

The `bin_rect` class implements boolean `contains` and `intersects`
methods. In addition to the simple boolean tests are methods,
`intersect_subset` that calculates the resulting rectangle `A ∩ B` and
`disjoint_subset` which subtracts the intersecting rectangle `A - A ∩ B`
to create up to four rectangles which may overlap. The disjoint set of
rectangles are used to index the MAXRECTS-BSSF algorithm free list.

```
struct bin_rect
{
    bin_point a, b;

    bool contains(bin_rect o)
    bool intersects(bin_rect o)
	vector<bin_rect> intersect_subset(bin_rect o);
	vector<bin_rect> disjoint_subset(bin_rect o);
};
```

### intersection types

Outside cases:

```
               o ooo
  /---\   /---\o  /---\   /---\
  |   |   |   |o  |   |   |   |
  |   |   |   |   |   |   |   | 
 o|   |   |   |   |   |   |   |
 o\---/   \---/   \---/   \---/
 o                           ooo
```

Fully inside case (0-axis crossing):

```

  /---\
  |   |
  | o |
  |   |
  \---/
```

Overlap cases (1-axis crossing):

```
                    o
  /---\   /---\   /-o-\   /---\
  |   |   |   |   | o |   |   |
  |   |  ooo  |   |   |   |  ooo
  | o |   |   |   |   |   |   |
  \ o-/   \---/   \---/   \---/
    o
```

Overlap cases (2-axis crossing):

```
         ooo         ooo
  /---\  ooo--\   /--ooo  /---\
  |   |  ooo  |   |  ooo  |   |
  |   |   |   |   |   |   |   |
 ooo  |   |   |   |   |   |  ooo
 ooo--/   \---/   \---/   \--ooo
 ooo                         ooo
```

Overlap cases (3-axis crossing):

```
 ooo     ooooooo     ooo
 ooo--\  ooooooo  /--ooo  /---\
 ooo  |  ooooooo  |  ooo  |   |
 ooo  |   |   |   |  ooo  |   |
 ooo  |   |   |   |  ooo ooooooo
 ooo--/   \---/   \--ooo ooooooo
 ooo                 ooo ooooooo
```

Overlap cases (3-axis crossing):

```
    o
  /-o-\   /---\
  | o |   |   |
  | o |  ooooooo
  | o |   |   |
  \-o-/   \---/
    o
```

Fully surrounded case (4-axis crossing):

```
 ooooooo
 o/---\o
 o|ooo|o
 o|ooo|o
 o|ooo|o
 o\---/o
 ooooooo
```
