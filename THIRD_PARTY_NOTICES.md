# Third-party notices

Phosphor Field's source code, tests, tools and documentation media are distributed under the
[MIT License](LICENSE). The repository contains no third-party source code, fonts, images or
audio.

## Libraries used at run time (not included)

Phosphor Field links dynamically against system libraries provided by your distribution. They
are not part of this repository or of the release archive.

| Library | Used for | License |
| --- | --- | --- |
| SDL2 (tested with sdl2-compat over SDL3) | Window, rendering, input, timers | zlib |
| SDL2_ttf (with FreeType and HarfBuzz) | Text rendering | zlib (SDL2_ttf); FreeType and HarfBuzz under their own licenses |
| C library and libm (e.g. glibc) | Runtime, math | LGPL-2.1-or-later with exceptions (glibc) |

Their license texts are installed with the respective packages.

## Fonts

No font is bundled. At run time the program opens one locally installed monospace TTF
(DejaVu Sans Mono, JetBrains Mono Nerd Font or Liberation Mono, or the file given in
`PHOSPHOR_FONT`). The screenshots show the font found on the test machine.

## Tools used only for tests and documentation (not linked)

Python 3 runs the integration and Aquarium tests. FFmpeg is used by `make showcase` to
encode the captured frames.

## Mathematics and creatures

PF-MEDUSA (`src/creature.h`) was designed and implemented for this project. It uses only
general mathematical methods (surfaces of revolution, golden-angle phyllotaxis, rotation
matrices, perspective projection, the normal-distribution CDF and Newton's method). No code,
constants or designs of other works were used. Development notes:
[docs/CREATURE_MEDUSA.md](docs/CREATURE_MEDUSA.md).

## Media in this repository

| File | Origin |
| --- | --- |
| `docs/media/demo.gif`, `tour.gif`, `medusa.png`, `lab.png`, `library.png`, `theme.png` | Captures of Phosphor Field itself, produced by [`tools/showcase/capture.sh`](tools/showcase/capture.sh). The two formula creatures in the tour (“Orbit Weave”, “Tide Ring”) were written for the showcase. |
| `docs/media/banner.svg` | Drawn for this project. The creature in it is 2 600 real PF-MEDUSA sample points. It names common monospace fonts for display; no font is embedded. |
| `tests/fixtures/sulfur-night.toml`, `paper-lab.toml` | Test palettes written for this project. |
