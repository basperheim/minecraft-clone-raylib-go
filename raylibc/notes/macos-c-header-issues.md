# macOS Wrong Path/Missing Header Issue

Here's a tight, practical post-mortem you can paste into your notes. I'll explain **what went wrong**, **why**, **how you diagnosed it**, the **exact fixes** you used, and a few durable tips so you don't waste time on the same thing again.

---

# Post-mortem: missing `raylib.h` on macOS (Homebrew)

## The problem (one line)

`cc hello_cube.c` failed with `fatal error: 'raylib.h' file not found` and `pkg-config: No package 'raylib' found` even though `brew install raylib` had already run.

## Root causes

1. **Compiler include path not set** — `clang` doesn't magically know Homebrew prefixes. It only searches standard system include dirs unless you pass `-I`.
2. **`pkg-config` couldn't find `raylib.pc`** because `PKG_CONFIG_PATH` didn't include Homebrew's `lib/pkgconfig`. On Apple Silicon Homebrew lives under `/opt/homebrew` (Intel uses `/usr/local`) and `pkg-config` won't see packages there by default.
3. Secondary: you tried to compile using only `-lraylib` and frameworks; without `-I` for headers the build fails earlier.

## How I diagnosed it (commands run)

These are the exact commands that find the problem and the files:

```bash
# Where Homebrew installed raylib
brew --prefix raylib
# Does the pkgconfig file exist?
ls "$(brew --prefix raylib)"/lib/pkgconfig/raylib.pc
# Does the header exist?
ls "$(brew --prefix raylib)"/include/raylib.h
# If pkg-config fails, inspect what it returns:
pkg-config --cflags --libs raylib   # (failed earlier)
```

If `pkg-config` says "no package found" but the `.pc` exists, your `PKG_CONFIG_PATH` is the missing link.

---

## Exact fixes (copy/paste)

### A — Temporary, quick compile (explicit paths)

This always works — tell the compiler where headers and libs live:

```bash
RP=$(brew --prefix raylib)
cc hello_cube.c -o hello_cube \
  -I"$RP/include" -L"$RP/lib" -lraylib \
  -framework Cocoa -framework OpenGL -framework IOKit
```

### B — Preferred: enable pkg-config (recommended)

Make pkg-config find raylib so you can use a one-liner:

```bash
export PKG_CONFIG_PATH="$(brew --prefix raylib)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
cc hello_cube.c -o hello_cube $(pkg-config --cflags --libs raylib)
```

To make that permanent add to your shell rc (`~/.zshrc` or `~/.bash_profile`):

```bash
# put one of these lines in ~/.zshrc
export PKG_CONFIG_PATH="$(brew --prefix raylib)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
```

Reload the shell or `source ~/.zshrc`.

### C — Use CMake (cross-platform)

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.15)
project(hello_cube C)
find_package(raylib REQUIRED)
add_executable(hello_cube hello_cube.c)
target_link_libraries(hello_cube PRIVATE raylib)
```

Build:

```bash
mkdir build && cd build
cmake ..           # CMake will use pkg-config or raylibConfig if available
cmake --build .
```

---

## Quick verification checklist (run these)

```bash
# confirm header & pc file
ls "$(brew --prefix raylib)"/include/raylib.h
ls "$(brew --prefix raylib)"/lib/pkgconfig/raylib.pc

# confirm pkg-config now sees raylib
export PKG_CONFIG_PATH="$(brew --prefix raylib)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
pkg-config --modversion raylib
pkg-config --cflags --libs raylib
```

If `pkg-config --modversion raylib` prints a version, you're golden.

---

## Extra notes & gotchas

- **Homebrew prefix differs by machine**

  - Apple Silicon: `/opt/homebrew`
  - Intel mac: `/usr/local`
    Always use `$(brew --prefix raylib)` — it's deterministic.

- **Runtime linking**
  If the program fails to run complaining about missing dynamic libs, the fix is similar: ensure lib is in the loader path. On macOS, Homebrew usually sets install names so this rarely happens; if it does, `install_name_tool` or adding `/opt/homebrew/lib` to `DYLD_FALLBACK_LIBRARY_PATH` during testing will help.

- **Framework flags** on macOS
  Raylib may require `-framework Cocoa -framework OpenGL -framework IOKit` when not pulled by pkg-config. Using `pkg-config` avoids having to think about these.

- **pkg-config missing?**
  Install it: `brew install pkg-config`.

- **Apple Silicon vs x86 builds**
  If you run into architecture mismatches (e.g., building x86_64 on ARM), you may need `arch -x86_64` to run tools under Rosetta or build raylib for the same arch as your compiler.

- **If headers exist but compiler still cannot find them**, double-check the include flag `-I` uses the exact path printed by `brew --prefix raylib` and that you didn't accidentally set a different SDK include path.

---

## Short recommended workflow (so it never bites you again)

1. `brew install raylib pkg-config`
2. `export PKG_CONFIG_PATH="$(brew --prefix raylib)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"` (put in shell rc)
3. Build with `cc hello_cube.c -o hello_cube $(pkg-config --cflags --libs raylib)` or use CMake.
4. If debugging: `pkg-config --cflags --libs raylib` first — read its output. If it's empty, fix `PKG_CONFIG_PATH`.

---

If you want, I'll generate a one-page cheat sheet file (Markdown) with these commands and the minimal `CMakeLists.txt` so you can paste it into your project. Want that?
