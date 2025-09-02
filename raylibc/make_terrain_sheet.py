#!/usr/bin/env python3
"""
make_terrain_sheet.py
Generate a whimsical 8x8 terrain sprite sheet (64x64 tiles by default)
and an index JSON mapping tile-name -> [col, row].

Outputs (by default):
 - terrain_sheet_simple.png
 - terrain_sheet_simple.json

Usage:
    python3 make_terrain_sheet.py
    python3 make_terrain_sheet.py --seed 42 --out terrain_sheet_simple.png --json terrain_sheet_simple.json
"""

from PIL import Image, ImageDraw, ImageFilter
import random
import json
import argparse
import os

TILE = 64
COLS, ROWS = 8, 8
W, H = TILE * COLS, TILE * ROWS

BASE_KINDS = [
    "grass", "flowers", "dirt", "sand",
    "stone", "water", "wood", "snow",
]

def make_grass(d):
    d.rectangle([0,0,TILE,TILE], fill=(95,170,95,255))
    for _ in range(70):
        x = random.randint(0, TILE-1)
        y = random.randint(0, TILE-1)
        d.point((x,y), fill=(70+random.randint(-5,5),135+random.randint(-20,20),70,200))
    # subtle darker bottom
    for y in range(int(TILE*0.7), TILE):
        for x in range(0, TILE, 6):
            if random.random() < 0.08:
                d.point((x,y), fill=(60,120,60,24))

def make_dirt(d):
    d.rectangle([0,0,TILE,TILE], fill=(139,105,80,255))
    for _ in range(40):
        r = random.randint(1,3)
        x,y = random.randint(0,TILE-1), random.randint(0,TILE-1)
        d.ellipse((x-r,y-r,x+r,y+r), fill=(90+random.randint(-10,10),70,55,170))

def make_sand(d):
    d.rectangle([0,0,TILE,TILE], fill=(230,220,170,255))
    for _ in range(50):
        x,y = random.randint(0,TILE-1), random.randint(0,TILE-1)
        d.point((x,y), fill=(210+random.randint(-10,10),190,140,200))

def make_stone(d):
    d.rectangle([0,0,TILE,TILE], fill=(150,150,150,255))
    for _ in range(18):
        r = random.randint(3,6)
        x,y = random.randint(4,TILE-5), random.randint(4,TILE-5)
        d.ellipse((x-r,y-r,x+r,y+r), fill=(120+random.randint(-10,10),120,120,240))

def make_water(d):
    d.rectangle([0,0,TILE,TILE], fill=(70,130,200,255))
    # soft horizontal bands
    for y in range(2, TILE, 8):
        d.line([(0,y),(TILE,y)], fill=(200,220,255,120), width=2)
    # small foam dots
    for _ in range(12):
        x = random.randint(0,TILE-1); y = random.randint(0,TILE-1)
        d.point((x,y), fill=(240,250,255,120))

def make_wood(d):
    d.rectangle([0,0,TILE,TILE], fill=(160,110,70,255))
    for y in range(0, TILE, 10):
        d.line([(0,y),(TILE,y)], fill=(110+random.randint(-10,10),80,50,255), width=2)
    # knots
    for _ in range(6):
        x = random.randint(6,TILE-6); y = random.randint(6,TILE-6)
        r = random.randint(1,3)
        d.ellipse((x-r,y-r,x+r,y+r), fill=(120,90,60,200))

def make_snow(d):
    d.rectangle([0,0,TILE,TILE], fill=(240,245,255,255))
    for _ in range(26):
        r = random.randint(1,2)
        x,y = random.randint(0,TILE-1), random.randint(0,TILE-1)
        d.ellipse((x-r,y-r,x+r,y+r), fill=(255,255,255,220))

def make_flowers(d):
    d.rectangle([0,0,TILE,TILE], fill=(100,170,100,255))
    colors = [(235,100,140),(255,210,120),(180,140,255),(255,180,220)]
    for _ in range(12):
        x,y = random.randint(5,TILE-6), random.randint(5,TILE-6)
        c = random.choice(colors)
        d.ellipse((x-2,y-2,x+2,y+2), fill=c+(255,))

KIND_TO_FUNC = {
    "grass": make_grass,
    "dirt": make_dirt,
    "sand": make_sand,
    "stone": make_stone,
    "water": make_water,
    "wood": make_wood,
    "snow": make_snow,
    "flowers": make_flowers,
}

def make_tile(kind):
    img = Image.new("RGBA", (TILE, TILE), (0,0,0,0))
    d = ImageDraw.Draw(img)
    if kind in KIND_TO_FUNC:
        KIND_TO_FUNC[kind](d)
    else:
        d.rectangle([0,0,TILE,TILE], fill=(180,180,180,255))
    # tiny painterly blur to soften edges (not too much)
    if random.random() < 0.25:
        img = img.filter(ImageFilter.GaussianBlur(radius=0.6))
    return img

def build_types_random(seed=None):
    rng = random.Random(seed)
    # Create a list of 64 tiles by sampling base kinds with small chance of variants.
    kinds = []
    for i in range(COLS*ROWS):
        # bias toward terrain basics but sometimes add flowers/snow etc.
        if rng.random() < 0.12:
            kinds.append(rng.choice(["flowers","snow"]))
        else:
            kinds.append(rng.choice(BASE_KINDS))
    # shuffle a bit to avoid vertical stacks
    rng.shuffle(kinds)
    return kinds

def make_sheet(out_png, out_json, seed=None):
    tile_types = build_types_random(seed)
    sheet = Image.new("RGBA", (W, H))
    index = {}
    for idx, kind in enumerate(tile_types):
        col = idx % COLS
        row = idx // COLS
        tile = make_tile(kind)
        sheet.paste(tile, (col*TILE, row*TILE), tile)
        index.setdefault(kind, []).append([col, row])
    sheet.save(out_png)
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump({"tile_size":TILE, "cols":COLS, "rows":ROWS, "mapping":index, "seed":seed}, f, indent=2)
    print(f"Saved {out_png} and {out_json} (seed={seed})")

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--seed", type=int, default=None, help="Random seed (optional)")
    p.add_argument("--out", default="terrain_sheet_simple.png", help="Output PNG filename")
    p.add_argument("--json", default="terrain_sheet_simple.json", help="Output JSON index filename")
    args = p.parse_args()
    make_sheet(args.out, args.json, args.seed)

if __name__ == "__main__":
    main()
