# Security and privacy

Phosphor Field is a local desktop program for a single user. It computes point fields and draws
them in a window. This page describes which files it touches, how untrusted input is handled
and how this was checked for version 0.6.0.

## Files

| Topic | Behaviour | How it is known |
| --- | --- | --- |
| Creature library | `$XDG_DATA_HOME/phosphor-field/library.pfl`, else `~/.local/share/phosphor-field/library.pfl`. Read at startup; the directory and file are created only when you save or assign a favourite. Writes take an exclusive non-blocking lock, check for external changes, write a temporary file with `fsync`, keep the previous version as `library.pfl.bak` and replace atomically. | Code; `make check` (persistence, conflict, lock, `ENOSPC`, file-size limit) |
| Settings | `settings.pfs` next to the library holds the Aquarium interval, written the same atomic way. Unknown or damaged files are left untouched and the setting is reported as volatile. | Code and tests |
| Damaged input | Unknown format versions, oversize files (over 256 KiB), symlinks and malformed entries are refused; writes are then blocked so nothing is overwritten. Files are never repaired automatically. | Tests for corrupt, oversize, symlink and legacy files |
| Logs | Off by default. `--log-dir DIR` writes only inside `DIR/phosphor-field/`, which must carry the program's own owner marker. Logs are bounded to three rotated files. Foreign or unmarked directories, symlinks, hard links and non-regular files are refused. Logs contain no key codes, user names, host names or full paths. | `make check-integration` |
| Screenshots | Written only for `--screenshot FILE.bmp` in a time-limited run. | Code |
| Theme | `--theme auto` reads `~/.local/state/omarchy/current/theme/colors.toml` and `theme.name` (at most 16 KiB). These files are only read; the desktop theme is never changed and no hooks are installed. | Code; integration tests use a private copy of that directory layout |
| Network | No networking code. | Code review of all file, process and socket calls |

## Formula language

Creatures from the Creation Lab or a library file are treated as untrusted input. They are
compiled to a small bytecode with at most 384 operations per point, 32 nesting levels, eight
parameters and 2 047 bytes of ASCII text. There are no loops, conditions, file access or
native code. Non-finite results freeze the last valid frame and are reported; they never crash
the program. A fuzz test compiles and evaluates 3 000 random inputs.

## Permissions

Phosphor Field runs as an ordinary user. It needs no root rights, setuid bits, capabilities,
device nodes or desktop portals, and it spawns no processes.

## Input handling

Command-line values are parsed strictly. Non-integers, NaN, infinity and out-of-range values
are rejected with exit status 1. Keyboard input changes only bounded settings. SIGTERM and SIGINT
cause an orderly shutdown with exit status 0. The test suites run under AddressSanitizer and
UndefinedBehaviorSanitizer.

## Build and test scripts

`make install` copies one file and `make uninstall` removes exactly that file. Tests and the
showcase tool use private `mktemp` directories and remove only what they created. They never
use your library, settings or desktop theme. Nothing is downloaded during build or test.

## Reporting a vulnerability

Please report suspected vulnerabilities privately through the repository's private
vulnerability reporting ("Security" tab) once it is enabled, rather than in a public issue.
Include the Phosphor Field version, your distribution and the steps to reproduce. If a library
file is needed, please reduce it to a minimal example without personal content.
