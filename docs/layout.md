# Text Layout

Placeholder for text layout documentation.

## Text Attributes

The following attributes are supported by `text_layout`.

- `font-family`
  - string e.g. Helvetica, Arial, Times, Times New Roman, etc.
- `font-style`
  - string e.g. Regular, Bold, Bold Italic, Italic, etc.
- `font-weight`
  - 100 = thin
  - 200 = extra light
  - 200 = light
  - 300 = light
  - 350 = semi light
  - 350 = book
  - 400 = normal
  - 400 = regular
  - 500 = medium
  - 600 = demibold
  - 600 = semibold
  - 700 = bold
  - 800 = extra bold
  - 800 = ultra bold
  - 900 = black
  - 900 = heavy
  - 950 = extra black
  - 950 = ultra black
- `font-slope`
  - 0 = none
  - 1 = oblique
  - 1 = italic
- `font-stretch`
  - 1 = ultra_condensed,
  - 2 = extra_condensed,
  - 3 = condensed,
  - 4 = semi_condensed,
  - 5 = medium,
  - 6 = semi_expanded,
  - 7 = expanded,
  - 8 = extra_expanded,
  - 9 = ultra_expanded,
- `font-spacing`
  - 0 = normal,
  - 1 = monospaced,
- `font-size`
  - integer points
- `baseline-shift`
  - integer pixels, y-shift, positive value raises
- `tracking`
  - integer pixels, y-spacing, added to character advance
- `line-spacing`
  - integer pixels, y-height, added for new lines
- `color`
  - HTML RGBA little-endian e.g. #ff000000 is black
- `language`
  - defaults to `en`, passed to HarfBuzz
