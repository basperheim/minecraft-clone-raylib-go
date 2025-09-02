# High-Level Wrapper

```
How difficult would it be to make it so a high-level abstraction, like in Typecript/Python, could decide where and how to render blocks like the Rust code does, but with less control (primary 3D Cartessian grid of blocks, etc.. is still handled at the Go-level)?
```

Short answer: it's very doable. Your Rust app already proves the pattern—**render loop on the native side, high-level control via a simple wire protocol**. Porting that idea to your Go/raylib voxel toy is straightforward for a small—mid world and modest update rates. The only "hard" bits show up when you want very large worlds or high-frequency per-voxel updates (then you'll need chunking, compression, and batching).

Below is the pragmatic way to do it, with concrete schemas and code skeletons.

---

# What to keep native vs. high-level

**Go/raylib (native, authoritative):**

- Owns the world (3D Cartesian grid / chunks)
- Meshing/drawing, frustum culling/LOD (you already have distance and cone culling)
- Input, camera, physics
- Small message queue to apply inbound world edits before each frame

**High-level (TypeScript/Python):**

- Decides _what_ to build and _where_ (structures, terrain, rules)
- Sends **batch commands** (not frame-by-frame drawing calls)
- Listens to events (keys, mouse, collisions, "player entered region", etc.)

---

# IPC: keep it boring

Start with **line-delimited JSON over stdin/stdout** (identical to your SDL2 wrapper). It's fast enough for prototypes and small worlds. If you outgrow it, switch payloads to MessagePack/CBOR without changing the command model.

- Inbound (TS/Python → Go): commands like `set`, `fill`, `column`, `chunk`.
- Outbound (Go → TS/Python): events like `key_down`, `mouse_button`, `tick`, `player_pos`.

---

# Minimal wire protocol (v0)

All messages are single-line JSON.

**Inbound ops**

```json
{"op":"init","world":{"size":[64,64,64]},"palette":{"GRASS":1,"STONE":2}}
{"op":"set","x":12,"y":18,"z":7,"id":"GRASS"}                    // single voxel
{"op":"fill","min":[0,0,0],"max":[15,3,15],"id":"STONE"}        // axis-aligned box
{"op":"column","x":10,"z":5,"h":20,"id":"GRASS"}                 // quick height col
{"op":"chunk","cx":2,"cz":-1,"y0":0,"dims":[16,64,16],
 "encoding":"rle8","data":"<base64>"}                           // bulk stream
{"op":"camera_set","pos":[32,28,80],"yaw":3.14,"pitch":-0.15}
{"op":"clear"}                                                  // wipe world
{"op":"flush"}                                                  // apply + ack
```

**Outbound events**

```json
{"event":"ready"}
{"event":"ack","op":"flush"}
{"event":"key_down","key":"W"}
{"event":"mouse_button","button":"Left","state":"down","x":123,"y":456}
{"event":"player","pos":[31.6,28.0,79.2],"yaw":3.18,"pitch":-0.14}
{"event":"quit"}
```

Notes:

- Use an **integer palette** for blocks. High-level can define symbols → ids once (`palette`), then send ints in heavy paths (e.g., `chunk`).
- `chunk` supports **RLE** or raw bytes to avoid chatty per-voxel updates.
- `flush` lets the script know "everything you sent so far was applied."

---

# Go side (raylib) — skeleton

Key ideas:

- **Reader goroutine** decodes JSON lines → typed `Cmd`.
- **Buffered channel** → main thread applies commands between frames (never block the draw loop).
- World representation: start with a simple 3D array; move to 16xYx16 chunks when needed.

```go
// go: world + command ingest
type BlockID = uint8

type World struct {
    Size [3]int
    Vox  []BlockID // flat: x + y*X + z*X*Y
    // Later: chunks map[[2]int]*Chunk
}

func (w *World) idx(x, y, z int) int { return x + y*w.Size[0] + z*w.Size[0]*w.Size[1] }

type Cmd struct {
    Op       string          `json:"op"`
    World    *struct{ Size [3]int } `json:"world,omitempty"`
    Palette  map[string]uint8 `json:"palette,omitempty"`
    X, Y, Z  int             `json:"x,omitempty","y,omitempty","z,omitempty"`
    Id       string          `json:"id,omitempty"`     // symbolic
    Min, Max [3]int          `json:"min,omitempty","max,omitempty"`
    H        int             `json:"h,omitempty"`
    Cx, Cz   int             `json:"cx,omitempty"`
    Y0       int             `json:"y0,omitempty"`
    Dims     [3]int          `json:"dims,omitempty"`
    Encoding string          `json:"encoding,omitempty"`
    Data     string          `json:"data,omitempty"`
    Pos      *[3]float32     `json:"pos,omitempty"`
    Yaw      *float32        `json:"yaw,omitempty"`
    Pitch    *float32        `json:"pitch,omitempty"`
}

type ApplyCtx struct {
    Palette map[string]BlockID
    World   *World
}

func startReader(cmdCh chan<- Cmd) {
    go func() {
        scanner := bufio.NewScanner(os.Stdin)
        for scanner.Scan() {
            var c Cmd
            if err := json.Unmarshal(scanner.Bytes(), &c); err != nil {
                fmt.Printf(`{"event":"error","msg":"%s"}`+"\n", strings.ReplaceAll(err.Error(), `"`, `\"`))
                continue
            }
            cmdCh <- c
        }
        fmt.Println(`{"event":"quit"}`)
    }()
}
```

Apply commands (between frames):

```go
func applyOne(ctx *ApplyCtx, c Cmd) {
    switch c.Op {
    case "init":
        sz := c.World.Size
        ctx.World = &World{Size: sz, Vox: make([]BlockID, sz[0]*sz[1]*sz[2])}
        ctx.Palette = make(map[string]BlockID)
        for k, v := range c.Palette { ctx.Palette[k] = v }
        fmt.Println(`{"event":"ready"}`)
    case "set":
        id := ctx.Palette[c.Id]
        i := ctx.World.idx(c.X, c.Y, c.Z)
        ctx.World.Vox[i] = id
    case "fill":
        id := ctx.Palette[c.Id]
        for z := c.Min[2]; z <= c.Max[2]; z++ {
            for y := c.Min[1]; y <= c.Max[1]; y++ {
                base := y*ctx.World.Size[0] + z*ctx.World.Size[0]*ctx.World.Size[1]
                for x := c.Min[0]; x <= c.Max[0]; x++ {
                    ctx.World.Vox[x+base] = id
                }
            }
        }
    case "column":
        id := ctx.Palette[c.Id]
        for y := 0; y < c.H; y++ {
            ctx.World.Vox[ctx.World.idx(c.X, y, c.Z)] = id
        }
    case "chunk":
        // decode c.Data (base64), then RLE or raw → apply into world at chunk coords
        // keep this path tight to avoid per-voxel overhead
    case "camera_set":
        // set camera variables for next frame
    case "clear":
        for i := range ctx.World.Vox { ctx.World.Vox[i] = 0 }
    case "flush":
        fmt.Println(`{"event":"ack","op":"flush"}`)
    }
}
```

Main loop: drain some commands, then render as you already do (with your culling/LOD):

```go
cmdCh := make(chan Cmd, 2048)
startReader(cmdCh)
ctx := &ApplyCtx{}

for !rl.WindowShouldClose() {
    // apply up to N commands per frame to bound latency
    for i := 0; i < 1000; i++ {
        select {
        case c := <-cmdCh: applyOne(ctx, c)
        default: i = 1000
        }
    }

    // input → events
    if rl.IsKeyPressed(rl.KeyW) { fmt.Println(`{"event":"key_down","key":"W"}`) }
    // ... mouse, etc.

    rl.BeginDrawing()
    rl.ClearBackground(rl.SkyBlue)
    rl.BeginMode3D(camera)
    // draw voxels using your LOD cone/distance test; sample:
    // for each cell that passes cull: if ctx.World.Vox[idx] != 0 draw
    rl.EndMode3D()
    rl.EndDrawing()
}
```

> Important: raylib's draw calls must stay on the main thread. Keep all JSON decoding in the goroutine and only mutate world state via the channel.

---

# TypeScript client (child process)

```ts
import { spawn } from "node:child_process";

const proc = spawn("./voxel_renderer", { stdio: ["pipe", "pipe", "inherit"] });

proc.stdout.setEncoding("utf8");
proc.stdout.on("data", (chunk) => {
  for (const line of chunk.trim().split("\n")) {
    try {
      console.log("event:", JSON.parse(line));
    } catch {}
  }
});

function send(obj: unknown) {
  proc.stdin.write(JSON.stringify(obj) + "\n");
}

send({ op: "init", world: { size: [64, 64, 64] }, palette: { AIR: 0, GRASS: 1, STONE: 2 } });
send({ op: "fill", min: [0, 0, 0], max: [15, 0, 15], id: "GRASS" });
send({ op: "column", x: 10, z: 10, h: 12, id: "STONE" });
send({ op: "flush" });
```

Python is the same idea with `subprocess.Popen(..., stdin=PIPE, stdout=PIPE)`.

---

# Performance reality check

- **JSON is fine** if you send _semantic_ ops (`fill`, `column`, `chunk`) and avoid per-block spam.
- For bigger worlds / frequent edits:

  - Use **chunked updates**; never send millions of `set`.
  - Allow `encoding:"rle8"` or `"raw8"`; base64 across the pipe is OK.
  - Keep `cmdCh` buffered and apply **N ops per frame** (bound latency).
  - Pre-allocate world/chunk arrays; avoid maps in hot loops.

- If you later need more: switch to **MessagePack** with the same op names; or transport over a **Unix domain socket/TCP** for remote scripting.

---

# Difficulty tiers (what actually gets hard)

- **Easy:** MVP exactly as above (few hundred LOC). Great for demos, world size \~64—128 cubed, scripted builds.
- **Medium:** Proper **chunking** (16xYx16), background meshing (optionally), block palette upgrades, and region unload/reload.
- **Hard:** Infinite worlds, threaded mesh generation, occlusion culling, networking, save formats, lighting/shadows.

You don't need the hard stuff to get a compelling, script-driven voxel sandbox running.

---

# Sanity checklist

1. Pick transport (start with stdin/stdout).
2. Freeze the **op schema** above (version it).
3. Implement Go reader goroutine + apply queue.
4. Make drawing consult the authoritative world array/chunks (your culling already works).
5. Emit events (keys/mouse/player pose) as JSON lines.
6. Write a 50-line TS/Py client that builds a test scene via `fill/column/chunk`.

If you want, I can draft the `chunk` RLE path or a tiny TS library (`RendererClient`) that wraps `send()` with type-safe methods.
