import ctypes as C, time, os, sys

# load the shared lib (adjust name per-OS)
if sys.platform == "darwin":
    lib = C.CDLL(os.path.abspath("./libmini3d.dylib"))
elif sys.platform.startswith("linux"):
    lib = C.CDLL(os.path.abspath("./libmini3d.so"))
elif sys.platform.startswith("win"):
    lib = C.CDLL(os.path.abspath("./mini3d.dll"))
else:
    raise RuntimeError("unsupported OS")

# declare arg/return types
lib.engine_create.restype = C.c_void_p
lib.engine_create.argtypes = [C.c_int, C.c_int, C.c_char_p, C.c_int]
lib.engine_destroy.argtypes = [C.c_void_p]
lib.engine_load_atlas.argtypes = [C.c_void_p, C.c_char_p, C.c_int, C.c_int, C.c_int]
lib.engine_load_atlas.restype  = C.c_bool
lib.engine_define_block_tile.argtypes = [C.c_void_p, C.c_uint16, C.c_int]
lib.engine_define_block_tile.restype  = C.c_bool
lib.engine_create_world.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int]
lib.engine_create_world.restype  = C.c_bool
lib.engine_set_block.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_uint16]
lib.engine_set_block.restype  = C.c_bool
lib.engine_fill_box.argtypes = [C.c_void_p, C.c_int,C.c_int,C.c_int, C.c_int,C.c_int,C.c_int, C.c_uint16]
lib.engine_tick.argtypes = [C.c_void_p, C.c_float]
lib.engine_tick.restype  = C.c_bool

# create engine
e = lib.engine_create(1280, 720, b"Mini3D - Python drives C", 60)

# load atlas (the one you generated earlier)
ok = lib.engine_load_atlas(e, b"terrain_sheet_simple.png", 64, 8, 8)
if not ok: raise RuntimeError("failed to load atlas")

# define tiles: 1..n map to first few atlas tiles (grass=0, flowers=1, dirt=2, sand=3, stone=4, water=5, wood=6, snow=7)
for bid, tidx in [(1,0),(2,1),(3,2),(4,3),(5,4),(6,5),(7,6),(8,7)]:
    lib.engine_define_block_tile(e, bid, tidx)

# world: 64x32x64
lib.engine_create_world(e, 64, 32, 64)

# build a simple scene: flat ground + a little pillar
lib.engine_fill_box(e, 0,0,0, 63,0,63, 1)  # grass floor
lib.engine_fill_box(e, 10,1,10, 10,5,10, 5) # stone pillar
lib.engine_fill_box(e, 20,1,20, 22,3,22, 3) # dirt lump
lib.engine_fill_box(e, 30,1,15, 30,8,15, 7) # snow post

# run loop: let C do input & rendering; Python just calls tick
prev = time.perf_counter()
while True:
    now = time.perf_counter()
    dt = now - prev
    prev = now
    if not lib.engine_tick(e, C.c_float(dt)):
        break

lib.engine_destroy(e)
