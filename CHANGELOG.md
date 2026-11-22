# Changelog

All notable changes to Phosphor Field. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

## 0.6.0 — 2026-11-22

First public release.

### Features
- **PF-MEDUSA**, the built-in creature: a 3D jellyfish with an asymmetric swim beat,
  delay-coupled tentacles, ruffled oral arms, a travelling bioluminescent light wave and a
  rare deep tilt. Stateless, deterministic, 2 500–50 000 points.
- **Creation Lab**: write creatures as `x`/`y` expressions with up to eight bounded named
  parameters; live preview on apply, rollback, undo/redo, line/column errors.
- **Creature Library**: up to 64 own creatures, atomic saves with backup, search, rename,
  delete, ten favourite slots.
- **Aquarium**: favourites take turns with a 1 s cross-fade, 5–120 s per creature.
- Instrument controls for points, speed, zoom, grid, persistence and decay; adaptive layout
  down to small tiling tiles; live Omarchy palette following or a fixed phosphor palette.
- Versioned JSON status stream (`--status-json`, schema 1) and opt-in bounded file logs.
- `make check`, `make sanitize`, `make check-integration`, `make check-aquarium`,
  `make showcase`, `make install` / `make uninstall` with `PREFIX`, `BINDIR` and `DESTDIR`.

### Fixed during release preparation
- Formula creatures in the Lab and the Aquarium were drawn off-centre once the native view
  was centred on (0,0). They now share the native frame, which a regression test pins.
- The window title showed the native creature's name while the Aquarium displayed a formula
  creature; it now names the creature on screen.
- Test binaries and QA output are written to `build/` instead of fixed paths in `/tmp`.
- Overriding `CFLAGS` on the `make` command line dropped `-std=c11` and the warnings.
