package main

import (
	"fmt"
	"math"

	rl "github.com/gen2brain/raylib-go/raylib"
)

// Fast deterministic value "noise" in [0,1) — placeholder for real Perlin/Simplex
func noise2D(x, y int) float32 {
	n := uint32(x*73856093 ^ y*19349663 ^ 0x9e3779b9)
	n ^= n << 13
	n ^= n >> 17
	n ^= n << 5
	return float32(n%10000) / 10000.0
}

func clampInt(v, lo, hi int) int {
	if v < lo {
		return lo
	}
	if v > hi {
		return hi
	}
	return v
}

func shadeGreen(y int, maxY int) rl.Color {
	if maxY < 1 {
		maxY = 1
	}
	t := float32(y) / float32(maxY)
	r := uint8(20 + 50*t)
	g := uint8(100 + 100*t)
	b := uint8(20 + 40*t)
	return rl.NewColor(r, g, b, 255)
}

func main() {
	const worldSize = 64
	const blockSize float32 = 1.0
	const eyeHeight float32 = 1.7

	// LOD radii (in blocks). Tweak these for FPS vs. quality.
	const nearR = 18
	const midR = 36
	const farR = 48 // anything beyond this is skipped

	// View-cone threshold: dot(forward, toCell) must be >= minDot to render
	const minDot float32 = -0.15 // allow a fairly wide cone; raise toward 0.2 to cull harder

	rl.InitWindow(1280, 720, "Go Minecraft Prototype (raylib-go) - Culling + LOD")
	defer rl.CloseWindow()
	rl.SetTargetFPS(60)

	// Camera yaw/pitch (radians)
	var yaw float32 = math.Pi
	var pitch float32 = -0.15
	const mouseSens float32 = 0.003

	camera := rl.Camera3D{
		Position:   rl.NewVector3(32, 28, 80),
		Target:     rl.NewVector3(32, 28, 79),
		Up:         rl.NewVector3(0, 1, 0),
		Fovy:       60.0,
		Projection: rl.CameraPerspective,
	}

	cursorLocked := true
	rl.DisableCursor()

	// Generate height map
	heightMap := make([]int, worldSize*worldSize)
	maxH := 0
	for z := 0; z < worldSize; z++ {
		for x := 0; x < worldSize; x++ {
			n := (noise2D(x, z) + noise2D(x+1, z) + noise2D(x, z+1) + noise2D(x+1, z+1)) * 0.25
			h := int(10 + n*22) // ~10..32
			heightMap[z*worldSize+x] = h
			if h > maxH {
				maxH = h
			}
		}
	}

	// Player physics
	var velY float32 = 0
	const gravity float32 = -18.0
	const jumpSpeed float32 = 6.5
	moveSpeed := float32(6.0)
	sprintMult := float32(1.8)

	for !rl.WindowShouldClose() {
		dt := rl.GetFrameTime()

		// Input toggles
		if rl.IsKeyPressed(rl.KeyTab) {
			cursorLocked = !cursorLocked
			if cursorLocked {
				rl.DisableCursor()
			} else {
				rl.EnableCursor()
			}
		}
		if rl.IsKeyPressed(rl.KeyP) {
			fmt.Printf("pos=(%.2f,%.2f,%.2f) yaw=%.2f pitch=%.2f\n",
				camera.Position.X, camera.Position.Y, camera.Position.Z, yaw, pitch)
		}

		// Mouse look
		if cursorLocked {
			d := rl.GetMouseDelta()
			yaw -= d.X * mouseSens
			pitch -= d.Y * mouseSens
			limit := float32(math.Pi / 3)
			if pitch > limit {
				pitch = limit
			}
			if pitch < -limit {
				pitch = -limit
			}
		}

		// Forward/right from yaw/pitch
		cp := float32(math.Cos(float64(pitch)))
		sp := float32(math.Sin(float64(pitch)))
		sy := float32(math.Sin(float64(yaw)))
		cy := float32(math.Cos(float64(yaw)))

		forward := rl.NewVector3(cp*sy, sp, -cp*cy)
		forward = rl.Vector3Normalize(forward)
		right := rl.Vector3Normalize(rl.Vector3CrossProduct(forward, camera.Up))

		// Movement
		speed := moveSpeed
		if rl.IsKeyDown(rl.KeyLeftShift) || rl.IsKeyDown(rl.KeyRightShift) {
			speed *= sprintMult
		}
		move := rl.NewVector3(0, 0, 0)
		if rl.IsKeyDown(rl.KeyW) {
			move = rl.Vector3Add(move, forward)
		}
		if rl.IsKeyDown(rl.KeyS) {
			move = rl.Vector3Subtract(move, forward)
		}
		if rl.IsKeyDown(rl.KeyA) {
			move = rl.Vector3Subtract(move, right)
		}
		if rl.IsKeyDown(rl.KeyD) {
			move = rl.Vector3Add(move, right)
		}
		if move.X != 0 || move.Y != 0 || move.Z != 0 {
			move = rl.Vector3Scale(rl.Vector3Normalize(move), speed*dt)
			camera.Position = rl.Vector3Add(camera.Position, move)
		}

		// Gravity + ground from heightmap under player
		velY += gravity * dt
		camera.Position.Y += velY * dt
		px := clampInt(int(math.Floor(float64(camera.Position.X+0.5))), 0, worldSize-1)
		pz := clampInt(int(math.Floor(float64(camera.Position.Z+0.5))), 0, worldSize-1)
		ground := float32(heightMap[pz*worldSize+px])
		minY := ground + eyeHeight
		onGround := false
		if camera.Position.Y <= minY {
			camera.Position.Y = minY
			velY = 0
			onGround = true
		}
		if onGround && rl.IsKeyPressed(rl.KeySpace) {
			velY = jumpSpeed
		}

		camera.Target = rl.Vector3Add(camera.Position, forward)

		// -------- drawing --------
		rl.BeginDrawing()
		rl.ClearBackground(rl.SkyBlue)

		rl.BeginMode3D(camera)
		rl.DrawGrid(64, 1.0)

		// Culling bounds in XZ around player
		minX := clampInt(px-farR, 0, worldSize-1)
		maxX := clampInt(px+farR, 0, worldSize-1)
		minZ := clampInt(pz-farR, 0, worldSize-1)
		maxZ := clampInt(pz+farR, 0, worldSize-1)

		near2 := nearR * nearR
		mid2 := midR * midR
		far2 := farR * farR

		var cubesDrawn int

		for z := minZ; z <= maxZ; z++ {
			for x := minX; x <= maxX; x++ {
				// Distance cull
				dx := x - px
				dz := z - pz
				dist2 := dx*dx + dz*dz
				if dist2 > far2 {
					continue
				}

				// View-cone cull (dot product)
				to := rl.NewVector3(float32(x)+0.5-camera.Position.X, 0, float32(z)+0.5-camera.Position.Z)
				to = rl.Vector3Normalize(to)
				dot := forward.X*to.X + forward.Z*to.Z // ignore Y for cone test
				if dot < minDot {
					continue
				}

				h := heightMap[z*worldSize+x]
				if h <= 0 {
					continue
				}

				if dist2 <= near2 {
					// NEAR: draw full column (blocky up close)
					for y := 0; y < h; y++ {
						pos := rl.NewVector3(float32(x), float32(y), float32(z))
						rl.DrawCube(pos, blockSize, blockSize, blockSize, shadeGreen(y, maxH))
						cubesDrawn++
					}
				} else if dist2 <= mid2 {
					// MID: draw only top block
					y := h - 1
					pos := rl.NewVector3(float32(x), float32(y), float32(z))
					rl.DrawCube(pos, blockSize, blockSize, blockSize, shadeGreen(y, maxH))
					cubesDrawn++
				}
				// FAR: skipped
			}
		}

		rl.EndMode3D()

		rl.DrawFPS(10, 10)
		rl.DrawText(fmt.Sprintf("cubes drawn: %d", cubesDrawn), 10, 30, 16, rl.Black)
		rl.DrawText("TAB: mouse lock | Shift: sprint | Space: jump", 10, 50, 16, rl.Black)

		rl.EndDrawing()
	}
}
