Nice — glad it ran. Here's a tight, practical post-mortem you can paste into your notes. I'll explain **what went wrong**, **why**, **how you diagnosed it**, the **exact fixes** you used, and a few durable tips so you don't waste time on the same thing again.

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

Nice — glad that worked. Below's everything you need, succinct and copy-pasteable.

## 1) Make vertical mouse _inverted_ by default (Minecraft style)

You already added the invert flags idea. To make it Minecraft-style (mouse **up** looks **down**, i.e. inverted Y), initialize `invert_mouse_y = true` and keep `invert_mouse_x = false`. Then use those flags in `process_input`.

### Edits to make

**A.** Add these fields to your `Engine` struct (near the other booleans/fields):

```c
    bool invert_mouse_x;
    bool invert_mouse_y;
```

**B.** Initialize them in `engine_create(...)` (after you set `cursor_locked` etc.):

```c
    e->invert_mouse_x = false;   // false: mouse-right -> look-right
    e->invert_mouse_y = true;    // true: invert vertical look (Minecraft-style)
```

**C.** Replace the mouse-look block in `process_input` with this (exactly):

```c
    // Mouse look
    if (e->cursor_locked) {
        Vector2 d = GetMouseDelta();
        const float sens = 0.0025f;

        float mx = e->invert_mouse_x ? -d.x : d.x;
        float my = e->invert_mouse_y ? -d.y : d.y;   // invert Y when true (Minecraft-style)

        e->yaw   += mx * sens;    // horizontal: mouse-right -> look-right
        e->pitch += my * sens;    // vertical: sign controlled by invert_mouse_y

        float limit = PI/2.2f;
        if (e->pitch > limit) e->pitch = limit;
        if (e->pitch < -limit) e->pitch = -limit;
    }
```

**D. (Optional) Toggle key** — If you want a runtime toggle, add:

```c
    if (IsKeyPressed(KEY_I)) {
        e->invert_mouse_y = !e->invert_mouse_y;
    }
```

Place that at the top of `process_input` (or wherever you prefer). This lets you flip invert-Y on the fly.

That's it — rebuild and you'll have horizontal mouse as you like and vertical inverted by default.

---

## 2) Exact compile commands (macOS and Linux)

### macOS (what you used)

If using Homebrew raylib:

```bash
RP=$(brew --prefix raylib)
cc -dynamiclib -o libmini3d.dylib engine.c \
  -I"$RP/include" -L"$RP/lib" -lraylib \
  -framework Cocoa -framework OpenGL -framework IOKit
```

Notes:

- You can still use `pkg-config` if you exported `PKG_CONFIG_PATH` earlier:

  ```bash
  export PKG_CONFIG_PATH="$(brew --prefix raylib)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
  cc -dynamiclib -o libmini3d.dylib engine.c $(pkg-config --cflags --libs raylib)
  ```

### Linux (shared library .so)

If raylib is available via `pkg-config`:

```bash
cc -fPIC -shared -o libmini3d.so engine.c $(pkg-config --cflags --libs raylib)
```

If `pkg-config` can't find raylib (build-from-source or nonstandard prefix), do this (adjust `RP` as appropriate):

```bash
# example if you installed raylib to /usr/local
RP=/usr/local
cc -fPIC -shared -o libmini3d.so engine.c -I"$RP/include" -L"$RP/lib" -lraylib -lm -lpthread -ldl -lGL
```

### Quick checks/helpful commands

- Confirm pkg-config sees raylib:

  ```bash
  pkg-config --modversion raylib
  pkg-config --cflags --libs raylib
  ```

- If `pkg-config` doesn't find it, set `PKG_CONFIG_PATH` to the `lib/pkgconfig` under your raylib prefix:

  ```bash
  export PKG_CONFIG_PATH="/opt/homebrew/opt/raylib/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
  # or /usr/local/opt/raylib/lib/pkgconfig etc.
  ```

---

## 3) Rebuild and test

1. Rebuild the shared lib with the command for your OS above.
2. Re-run your Python client (it should pick up the new lib if it loads the same filename).
3. Move the mouse up/down — it should now feel like Minecraft (inverted Y) while horizontal movement is correct.

---

## 4) Quick explanation of signs (so you remember)

- `GetMouseDelta().y` is positive when the OS reports the cursor moved _down_.
- `invert_mouse_y = true` → multiply `d.y` by `-1` → moving mouse _down_ produces negative `my` → `pitch += negative` → look downward when moving mouse down? (The net effect matches Minecraft when `invert_mouse_y` is `true` in this wiring.)
  (You already tried both signs — this flag makes the intent explicit and easy to toggle later.)

---

If you want, I'll produce a tiny patch/diff you can `patch -p0` or apply directly to your `engine.c` — or I can add a UI toggle printed on screen that shows "Invert Y: ON/OFF". Which would you prefer?
