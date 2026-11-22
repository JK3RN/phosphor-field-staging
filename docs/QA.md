# QA report — Phosphor Field 0.6.0

The automated tests were run on 2026-11-22 against a clean export of the release tree (no build
output and no local data).

## Environment

| Item | Value |
| --- | --- |
| OS | Omarchy (Arch-based), Linux 7.2, x86_64 |
| Compiler | GCC 16.2.1 |
| SDL | sdl2-compat 2.32.74 (SDL2 API on SDL3), SDL2_ttf 2.24.0 |
| Python | 3.14.8 (test scripts only) |
| Display for automated runs | SDL `dummy`/`offscreen` drivers with the software renderer. **No GPU or compositor is involved in these numbers.** |

## Automated results

| Target | Result | What it covers |
| --- | --- | --- |
| `make` | PASS, 0 warnings | `-std=c11 -Wall -Wextra -Wpedantic`, also with `CFLAGS` overridden on the command line |
| `make check` | PASS | 7 suites, `--version`/`--help` and three headless app runs. The creature contract covers 699 209 samples: finite, inside the view, bit-identical on repeat, continuous across the beat wrap, with a monotone light packet. Also tested: selection and keys, layout and DPI, theme parsing and contrast, log rotation, ownership and `ENOSPC`, the formula parser with a 3 000-input fuzz, persistence (atomic writes, conflict, lock, corrupt, oversize, symlink, legacy library refused read-only) and the Aquarium. Headless runs at 10 000 and 50 000 points. |
| `make sanitize` | PASS | All suites plus 3 s app runs at 50 000 points and in `--qa` mode under ASan and UBSan, with no findings. Leak detection is off because of SDL and driver noise. |
| `make check-integration` | PASS, 15 checks | JSON schema, sampling, signals, closed and slow status pipes, refused foreign or symlinked log targets, strict CLI parsing, init failure without a false `ready`, and live palette swaps with invalid-file retention |
| `make check-aquarium` | PASS | Real event loop at 1920×1080 with mixed native and formula favourites, pause, resize, live theme and restore |
| `make install` / `make uninstall` with `DESTDIR` | PASS | Exactly one file installed and then removed |
| Missing dependency | PASS | `make` stops with a message naming SDL2/SDL2_ttf |
| `make showcase` | PASS | All six media files reproduced **byte-identically** |

### Measurements (software renderer, headless, indicative only)

| Run | Result |
| --- | --- |
| 760×600, 10 000 points | 58.3 fps, mean frame work 5.6 ms |
| 760×600, 50 000 points | 58.0 fps, mean frame work 10.0 ms |
| PF-MEDUSA maths alone | 2.4 ms per frame at 12 000 points, 9.3 ms at 50 000 |
| Aquarium cross-fade at 1920×1080, 10 000 points | 43 fps during the fade, 54 fps overall |
| Aquarium cross-fade at 1920×1080, 50 000 points | 35 fps during the fade, 47 fps overall |

On a GPU renderer the fades are expected to be cheaper, but this was not measured.

## Manual checks

| Check | Result |
| --- | --- |
| PF-MEDUSA on the maintainer's Omarchy/Hyprland desktop (visual acceptance) | PASS (maintainer, 2026-11-22) |
| Creation Lab and Aquarium on a real desktop after the off-centre fix | **Pending** human acceptance |

## Not tested

Not tested for this release: other distributions, X11 sessions, classic (non-compat) SDL2, Clang,
fractional scaling, multi-monitor setups, long soak runs on a GPU, and VRAM or leak behaviour over
hours.
