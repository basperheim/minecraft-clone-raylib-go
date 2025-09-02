# mini3d — tiny C engine + Python controller (example)

Minimal, practical example showing how to run a small raylib-based C engine as a shared library and drive it from Python via `ctypes`.
This repo is a _reference/demo_, not a full game engine — but it contains a usable Minecraft-like prototype (dense voxels, colored cubes, basic first-person controls) and the exact pieces you need to extend it.

> TL;DR: build `libmini3d` (C/raylib), put `terrain_sheet_simple.png` next to the Python script, run `python3 test_client.py`. C renders & handles input/physics; Python decides what blocks exist and calls the C API.

---

## What this example does

- C engine (`engine.c`)

  - Opens window, input sampling, camera, basic physics (gravity + jump).
  - Stores a small voxel world (dense array).
  - Renders the world (naive draw-every-block approach — good for small demos).
  - Loads a sprite/terrain atlas and slices it into tiles (optional).
  - Exposes a small C API for creation/destruction, world edits, atlas loading and ticking frames.

- Python client (`test_client.py`)

  - Loads the engine shared library via `ctypes`.
  - Loads/creates a sprite sheet (optional script included).
  - Calls API functions to create the world, place blocks, and loop the engine by calling `engine_tick()` each frame.

Why this split? Keep frame-critical code in C for performance; keep game logic / rules / iteration speed in Python.

---

## Prerequisites

### Tools & libs

- C compiler

  - macOS: `cc` (clang), Homebrew toolchain recommended
  - Linux: `gcc`/`clang`
  - Windows: MSYS2 MinGW-w64 (`pacman` packages)

- raylib (C library) installed for your platform

  - macOS: `brew install raylib pkg-config`
  - Ubuntu: `sudo apt install libraylib-dev pkg-config` (or build raylib from source)
  - Windows (MSYS2): `pacman -S mingw-w64-x86_64-raylib pkg-config`

- Python 3

  - No special binding packages required — we use the stdlib `ctypes`.
  - Optional: `pip install pillow` if you want to generate the sprite-sheet PNG from the included generator.

- `pkg-config` recommended (easier compile lines)

---

## Files of interest

- `engine.h` — public C API header (what Python calls)
- `engine.c` — engine implementation (raylib + logic + rendering)
- `test_client.py` — Python example client using `ctypes`
- `make_terrain_sheet.py` — Pillow script to create `terrain_sheet_simple.png` and JSON index
- `terrain_sheet_simple.png` (generated or provided) — atlas used by the example
- `README.md` — this file

---

## Build (shared library)

> Build commands assume repo root contains `engine.c` and `engine.h`. Replace paths/names as needed.

### macOS (Homebrew)

```bash
# ensure raylib installed: brew install raylib pkg-config
RP=$(brew --prefix raylib)        # e.g. /opt/homebrew/opt/raylib
export PKG_CONFIG_PATH="$RP/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

# Quick with pkg-config (preferred if it finds raylib)
cc -dynamiclib -o libmini3d.dylib engine.c $(pkg-config --cflags --libs raylib)

# Or explicit include/link (works without pkg-config)
cc -dynamiclib -o libmini3d.dylib engine.c \
  -I"$RP/include" -L"$RP/lib" -lraylib \
  -framework Cocoa -framework OpenGL -framework IOKit
```

### Linux (pkg-config)

```bash
# ensure raylib is installed and visible to pkg-config
cc -fPIC -shared -o libmini3d.so engine.c $(pkg-config --cflags --libs raylib)
```

If `pkg-config` fails, set `PKG_CONFIG_PATH` to the directory containing `raylib.pc` or pass `-I`/`-L` manually.

### Windows (MSYS2 / MinGW64)

Open MinGW64 shell:

```bash
pacman -S mingw-w64-x86_64-raylib mingw-w64-x86_64-toolchain pkg-config
gcc -shared -o mini3d.dll engine.c $(pkg-config --cflags --libs raylib) -Wl,--out-implib,libmini3d.a
```

### CMake alternative

If you prefer CMake, a minimal `CMakeLists.txt` is recommended (use `find_package(raylib REQUIRED)` or `pkg-config`).

---

## Generate / provide the sprite sheet (optional)

If you want the sample atlas:

```bash
python3 make_terrain_sheet.py     # defaults -> terrain_sheet_simple.png + .json
# or deterministic:
python3 make_terrain_sheet.py --seed 42
```

Place `terrain_sheet_simple.png` next to `test_client.py` or pass an absolute path in the Python script.

---

## Run the demo (Python)

1. Build the shared lib (see above). Put the produced `libmini3d.dylib` / `libmini3d.so` / `mini3d.dll` in the same directory as `test_client.py`, or load with an absolute path.
2. Make sure the atlas PNG (`terrain_sheet_simple.png`) is present (or edit `test_client.py` to point to your asset).
3. Run:

```bash
python3 test_client.py
```

`test_client.py` will:

- load the shared lib with `ctypes`
- call `engine_create(...)`, `engine_load_atlas(...)`, define block tile mappings
- create a small world and place some blocks
- run a loop that calls `engine_tick(dt)` repeatedly until the window closes

---

## C API (short reference)

These are the exported functions used by the Python client (types in C):

```c
Engine* engine_create(int width, int height, const char* title, int target_fps);
void    engine_destroy(Engine* e);

bool engine_load_atlas(Engine* e, const char* png_path, int tile_px, int cols, int rows);
bool engine_define_block_tile(Engine* e, uint16_t block_id, int tile_index);

bool engine_create_world(Engine* e, int sx, int sy, int sz);
void engine_clear_world(Engine* e, uint16_t block_id);

bool engine_set_block(Engine* e, int x, int y, int z, uint16_t block_id);
uint16_t engine_get_block(Engine* e, int x, int y, int z);

void engine_fill_box(Engine* e, int x0,int y0,int z0, int x1,int y1,int z1, uint16_t block_id);

bool engine_tick(Engine* e, float dt); // returns false to request quit
```

See `engine.h` for exact typedefs and any extras (camera setters/getters, etc.). Keep FFI calls coarse: avoid calling per-block in tight loops — instead batch edits.

---

## Python usage example (ctypes snippet)

```python
import ctypes as C, os, sys, time

libpath = os.path.abspath("./libmini3d.dylib")  # change extension per OS
lib = C.CDLL(libpath)

# prototypes (example)
lib.engine_create.restype = C.c_void_p
lib.engine_create.argtypes = [C.c_int, C.c_int, C.c_char_p, C.c_int]
lib.engine_destroy.argtypes = [C.c_void_p]
# ... declare other argtypes/restypes like in test_client.py

e = lib.engine_create(1280, 720, b"Mini3D", 60)
ok = lib.engine_load_atlas(e, b"terrain_sheet_simple.png", 64, 8, 8)
# define tiles, create world, fill boxes...
while lib.engine_tick(e, C.c_float(1.0/60.0)):
    pass
lib.engine_destroy(e)
```

---

## Design & responsibilities (who does what)

**C engine**

- Performance-critical: rendering, camera update, input sampling, world storage, chunk meshing (future), picking/raycast.
- Exposes a small, stable C API (opaque Engine pointer).
- Keeps the hot loop internal and returns control to caller via `engine_tick()`.

**Python client**

- Game logic, rules, assets orchestration: worldgen, inventory, crafting, UI decisions, spawn logic.
- Calls coarse C API methods; handles higher-level decisions and data structures.
- Great for fast iteration and experimentation.

---

## Performance tips / guidance

- Keep the ABI small and coarse: call C once per meaningful action (e.g., per chunk edit, per frame), not per block in hot code paths.
- Implement chunking + mesh generation in C for anything larger than toy worlds. The example draws every cube and will not scale.
- Use `ImageFromImage()` slicing once (as in the example) to create GPU textures, then draw or assign them to model materials — avoid CPU↔GPU uploads per frame.
- Batch block edits with a single call if you need to terraform large areas.

---

## Packaging & distribution notes

- License: **raylib** is zlib/libpng licensed — permissive. Your game code is your IP; no copyleft.
- When shipping, include the raylib license with the raylib source in your repo if you redistribute the library source. Not required to show it to end users but good practice to include an acknowledgement file.
- On macOS, if you build a `.dylib`, ensure correct install names or bundle it with the app package. On Linux, consider `LD_LIBRARY_PATH` or packaging the `.so` into the application directory.

---

## Troubleshooting (common issues)

- **`raylib.h` not found**: either raylib isn’t installed or compiler can’t find headers. Use `brew --prefix raylib` on macOS and pass `-I"$RP/include"`, or set `PKG_CONFIG_PATH` and use `$(pkg-config --cflags --libs raylib)`.
- **`Package raylib was not found`**: set `export PKG_CONFIG_PATH="$(brew --prefix raylib)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"`.
- **Python can’t find PNG**: Python process `cwd` may differ. Use absolute path or `os.path.join(os.path.dirname(__file__), 'terrain_sheet_simple.png')`.
- **Missing symbols (DrawCubeTexture / FILTER_BILINEAR)**: older/newer raylib variations may rename or not include convenience helpers. The example falls back to colored cubes for portability.
- **High CPU / poor FPS**: naive per-cube drawing is expensive. Implement chunk meshing and face culling.

---

## Extending this example

Ideas to make it a real prototype engine:

- Implement chunking (e.g., 16×16×256), per-chunk greedy meshing in C.
- Replace colored cubes with textured cube models (assign per-face UVs).
- Expose event queue (pick events) for Python to consume.
- Add networked multiplayer via a Python server that uses the same C ABI for clients.
- Move inventory, crafting, entity AI to Python; keep collision & physics in C.

---

## License

- The example C code here (your code) — pick your license (MIT recommended for examples).
- raylib: zlib/libpng license (permissive). Include the license text if you redistribute raylib source.

---

## Final notes

- This repo is intended to be a practical starting point: **fast to iterate** (Python), **fast to run** (C for render/physics).
- Use the split to prototype gameplay in Python quickly, then move hot code to C/C++ when profiling demands it.
