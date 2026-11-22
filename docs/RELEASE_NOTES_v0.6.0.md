# Phosphor Field 0.6.0

First public release of Phosphor Field, a native Linux instrument for animated mathematical
point fields, and of its built-in creature **PF-MEDUSA**.

## Highlights

- **PF-MEDUSA**: a 3D jellyfish whose swim stroke travels down its tentacles and whose light
  wave runs from apex to tentacle tips on every beat.
- **Creation Lab**, **Creature Library** and **Aquarium**: write your own creatures, keep
  them on favourite keys and let them take turns with cross-fades.
- Small and private: one C11 binary on SDL2/SDL2_ttf, no network code, nothing written until
  you save, logs only on request.

## Install

Build from the source archive or a clone (see the README for dependencies):

```sh
make && ./phosphor-field
make install PREFIX=$HOME/.local   # optional
```

No prebuilt binary is provided: it would depend on your distribution's SDL2 build.

## Verify the download

```sh
sha256sum -c SHA256SUMS
```

## Tested

Omarchy (Arch) x86_64, Hyprland/Wayland, GCC 16.2, sdl2-compat 2.32, SDL2_ttf 2.24.
Other distributions and X11 are untested. Full results: [docs/QA.md](QA.md).

## Known limitations

See the README. In short: no 60 fps guarantee at 50 000 points with heavy formulas, theme
following targets Omarchy's palette file, ASCII-only names/formulas, and libraries from
unreleased development builds are refused read-only.
