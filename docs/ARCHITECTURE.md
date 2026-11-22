# Architecture and reference

Phosphor Field is about 1 800 lines of C11 in `src/`, compiled as one translation unit plus two
support files. It uses SDL2 for the window, rendering and input and SDL2_ttf for text.

```text
                 ┌──────────────── frame loop (≤ 60 fps, time-based) ────────────────┐
keys ─► input ─► │ settings ─► generator(N, phase) ─► project + clip ─► SDL points    │
                 │                 ▲  native: PF-MEDUSA                 │             │
                 │                 └─ formula: Lab / Aquarium bytecode  ▼             │
                 │        phosphor trail texture (exponential decay) + grid + HUD    │
                 └───────────────────────────────────────────────────────────────────┘
   theme poller (1 Hz) · status JSON (stdout) · optional bounded file log · library I/O
```

| File | Responsibility |
| --- | --- |
| `src/main.c` | CLI, main loop, layout, rendering, input, status snapshots |
| `src/field.h` | Shared types (`Point`, `Settings`), defaults and the phase clock |
| `src/generator.h` | Registry of compiled-in creatures (`NATIVE_COUNT`, view frame) |
| `src/creature.h` | PF-MEDUSA, see [CREATURE_MEDUSA.md](CREATURE_MEDUSA.md) |
| `src/formula.h` | Formula parser, bytecode compiler and evaluator |
| `src/lab.h` | Creation Lab: editor, parameters, apply/rollback, library browser |
| `src/library.h` | Creature library and favourites: format, validation, atomic writes |
| `src/aquarium.h` | Aquarium mode: slot rotation, cross-fade, interval setting |
| `src/theme.c/.h` | Palette reader (Omarchy flat palette), contrast rules, live polling |
| `src/observe.c/.h` | Status/log records, rotation, timing histograms, CPU/RSS sampling |

## Coordinates and time

- Every creature writes `N` points into a caller-owned buffer and keeps no state between frames. The view frame is centred on (0,0) with a half-span of 200 units and a grid step of 25. Y grows downwards. Points outside the plot are counted as `clipped`, and non-finite points as `invalid`.
- Formula creatures use the same frame, so `x = 0, y = 0` is the plot centre. A test pins this.
- The phase `t` advances by 0.75π per second at speed 1. Pausing or hiding the window stops it, and gaps are capped at 100 ms.
- Persistence is a separate trail texture faded exponentially by the decay time constant. Grid and text are composed on top and are never smeared.

## Formula language

```text
param <name> = <value> [<min>,<max>,<step>]     # up to 8, before x and y
x = <expression>
y = <expression>
```

Expressions use numbers, `pi`, `i` (sample index 0…N−1), `u` (= 2π·i/N), `t` (phase), the
parameters, `+ - * /`, unary minus, parentheses, `sin cos abs sqrt pow(a,b) min(a,b) max(a,b)`.
Names are ASCII, at most 23 characters, start with a letter and may not shadow variables or
functions. Each parameter's value must lie inside its finite bounds, and the step must be greater
than 0. Limits are 2 047 bytes, 384 bytecode operations per point and 32 nesting levels.
Library formulas must evaluate finitely at phase 0 with N = 10 000 so that they load
reproducibly; other phases and point counts are checked every frame.

Draft, applied and saved are separate states. `Ctrl+S` applies, `Ctrl+D` saves. An invalid
draft reports line and column and never replaces the running preview. A runtime failure (for
example division by zero at some phase) freezes the last complete frame until you edit, roll
back (`F7`) or reset (`Ctrl+R`).

## Library file

`PHOSPHOR_LIBRARY 1`. The file holds ten favourite mappings, a count, then entries with an ID
(`c-` and 32 hex digits) and explicit byte lengths for name and formula. Limits: 64 own creatures,
names up to 79 bytes and the whole file up to 256 KiB. The native creature is referenced by its
fixed ID `pf-medusa`. Writing follows lock → conflict check → temp file + `fsync` → previous
version to `.bak` → atomic rename → directory `fsync`. If anything fails before the rename, the
old library stays intact.

## Command line

| Option | Meaning |
| --- | --- |
| `--help`, `--version` | Text output; no display needed |
| `--size W H` | Initial size 160–4096 × 120–4096 (the window manager may override it) |
| `--points N` | Initial point count 1–50 000 |
| `--seconds N` | Finite run (0 = unlimited) |
| `--screenshot FILE.bmp` | Capture the last frame of a time-limited run |
| `--theme auto\|phosphor` | Follow the Omarchy palette (default) or use the fixed phosphor palette |
| `--theme-file FILE` / `--theme-current DIR` | Read and watch another palette / another Omarchy `current` directory |
| `--status-json` | Versioned JSON Lines status on stdout |
| `--sample-seconds N` / `--no-telemetry` | Periodic sample interval 0.1–3600 s / periodic samples off |
| `--log-dir DIR`, `--log-max-bytes N` | Opt-in bounded file log (4 KiB–64 MiB per file, three files) |
| `--input-debug`, `--qa` | Key diagnostics on stderr / scripted control sequence for tests |

Exit status 0 is returned for normal quit, timeout and SIGTERM/SIGINT. Exit status 1 is returned
for invalid arguments and for initialisation or render failures.

## Status stream (schema 1)

With `--status-json`, stdout carries one JSON object per line:

```json
{"schema":1,"program":"phosphor-field","version":"0.6.0","run_id":"…","time":"…Z","elapsed_s":5.0,"severity":"info","event":"sample","data":{}}
```

- **Events.** `ready` follows the first presented frame. `sample` is periodic and carries `fps`, CPU %, a work histogram and a state snapshot. `generator_changed` marks a switch. `exit` carries the reason, the code and the full statistics.
- **State snapshot.** It contains the active and the last calculated creature, point counts (`calculated`, `drawn`, `invalid`, `clipped`), phase, zoom, speed, decay, theme, cache counters and the Aquarium state.
- **Slow readers.** A slow reader never blocks rendering. Records that do not fit are dropped whole and counted in `status_dropped`.
- **Compatibility.** Consumers should tolerate unknown events and fields and handle other schema versions explicitly.

## Themes

The palette reader accepts flat `key = "#RRGGBB"` files. It requires `background`, `foreground` and
`accent`; `yellow` (else `color3`) is optional, and unknown keys are ignored. The reader polls once
per second and needs two identical reads before switching, so a half-written file is never
applied. If a palette is invalid, the last valid one is kept. Contrast is corrected to at least
4.5:1 for main text and 3:1 for secondary labels. Light desktop themes are mapped to a dark
instrument surface so that additive persistence keeps working.
