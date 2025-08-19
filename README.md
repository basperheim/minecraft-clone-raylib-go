# Minecraft Clone: raylib-go

- Minecraft clone written in Golang with [raylib-go](https://github.com/gen2brain/raylib-go)
- Minimal **Minecraft-like** voxel prototype in Go using **raylib-go**.
- Features: WASD + mouse look, jump/gravity, heightmap terrain, **culling + LOD** so it runs smoothly on modest hardware.

![Minecraft clone written in Go-Raylib video](https://github.com/user-attachments/assets/1fb45357-23e3-49f1-9c12-669f19c77d2e)

---

## Requirements

- **Go** 1.21+ (CGO enabled)
- **raylib (native C library)**
- A working C toolchain and `pkg-config`

### macOS (Homebrew, incl. Apple Silicon)

```bash
brew install cmake pkg-config
brew install raylib
# If pkg-config can't find raylib on Apple Silicon:
# export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig"
```

### Debian/Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y build-essential git pkg-config cmake
sudo apt-get install -y libraylib-dev
```

### Windows (MSYS2 / MinGW64 shell)

1. Install MSYS2: [https://www.msys2.org/](https://www.msys2.org/)
2. In **MSYS2 MinGW64** shell:

```bash
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-pkgconf mingw-w64-x86_64-raylib
```

3. Ensure `C:\msys64\mingw64\bin` is on your Windows `PATH` when running `go run`.

---

## Quick Start

```bash
git clone git@github.com:basperheim/minecraft-clone-raylib-go.git
cd minecraft-clone-raylib-go
go mod tidy

# Run
go run .

# Build
go build -o voxel-minecraft .
./voxel-minecraft   # Windows: .\voxel-minecraft.exe
```

---

## Controls

- **W/A/S/D**: move
- **Mouse**: look around (cursor locked by default)
- **Space**: jump
- **Shift**: sprint
- **Tab**: toggle mouse lock
- **P**: print camera position/yaw/pitch to console

---

## What's implemented

- **Heightmap terrain** (value-noise "good enough" for a prototype)
- **Gravity & jump** with ground sampled from the terrain under the player
- **Culling + LOD**:

  - **Near radius** → draw full columns (chunky)
  - **Mid radius** → top block only
  - **Far** → skipped

- **Simple height-based tint** (makes columns readable even without a shader)
- **Frame-rate independent** movement/physics (`GetFrameTime()`)

> Why not goroutines for draw? Drawing is GPU-bound; goroutines help later for **worldgen + chunk meshing**, not for issuing more draw calls.

---

## Performance knobs

Open `main.go` and tweak:

```go
const nearR = 18    // draw full columns within this radius
const midR  = 36    // draw only the top block out to here
const farR  = 48    // skip beyond this
const minDot float32 = -0.15 // view-cone culling (raise toward 0.2 to cull harder)
```

Tighten radii and raise `minDot` for higher FPS.

---

## Roadmap

- **Chunk meshing** (exposed-face extraction, one VBO per chunk)
- **Greedy meshing** (merge coplanar faces to slash triangles)
- **Frustum culling** via chunk AABBs
- **Texture atlas** (dirt/grass/stone) + basic directional light shader
- **Collision** with AABB instead of heightmap-only ground
- **Streaming** chunks and background meshing via goroutines
- Optional: **wasm** build (Emscripten) to run in browser

---

## Troubleshooting

- **Undefined references / linker errors** → ensure raylib is installed and `pkg-config --libs raylib` returns flags.
- **CGO disabled** → `go env -w CGO_ENABLED=1`.
- **Apple Silicon** → set `PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig`.
- **Windows** → run commands from **MSYS2 MinGW64** shell or ensure MinGW64 `bin` on `PATH`.

---

## Credits

- Go bindings: [https://github.com/gen2brain/raylib-go](https://github.com/gen2brain/raylib-go)
- Raylib (C): [https://github.com/raysan5/raylib](https://github.com/raysan5/raylib)
- Raymath cheatsheet: [https://www.raylib.com/cheatsheet/raymath_cheatsheet.html](https://www.raylib.com/cheatsheet/raymath_cheatsheet.html)
