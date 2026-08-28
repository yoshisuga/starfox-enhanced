#!/usr/bin/env python3
"""Generates the iOS application icon.

The artwork is deliberately generic geometry rather than any character or
craft from the cartridge: a flat-shaded polygonal fighter over a vanishing
-point grid, which evokes the Super FX look without reproducing protected
art. Drawn at 4x and downsampled so the polygon edges stay clean.
"""
from PIL import Image, ImageDraw

SIZE = 1024
SS = 4  # supersampling factor
W = SIZE * SS

# A restrained palette in the spirit of the SNES original: deep space blues
# behind a cool metal fighter, with one warm accent for the engine.
SPACE_TOP = (8, 10, 28)
SPACE_BOTTOM = (26, 18, 58)
GRID = (58, 74, 168)
GRID_BRIGHT = (96, 128, 232)
HULL_LIGHT = (206, 214, 230)
HULL_MID = (146, 158, 184)
HULL_DARK = (86, 96, 124)
WING_LIGHT = (74, 150, 226)
WING_DARK = (38, 88, 158)
ENGINE = (255, 176, 64)
ENGINE_CORE = (255, 238, 196)


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def main() -> None:
    image = Image.new("RGB", (W, W), SPACE_TOP)
    draw = ImageDraw.Draw(image)

    # Vertical space gradient.
    for y in range(W):
        draw.line([(0, y), (W, y)], fill=lerp(SPACE_TOP, SPACE_BOTTOM, y / W))

    # Perspective grid converging on a horizon above centre. It is scenery,
    # not subject: at 60px the ship has to win, so the grid stays low contrast
    # and coarse enough that it never turns into moire.
    horizon = int(W * 0.52)
    vanish_x = W // 2
    for step in range(-8, 9):
        x_bottom = vanish_x + step * int(W * 0.26)
        draw.line([(x_bottom, W), (vanish_x, horizon)], fill=GRID, width=SS * 3)
    spacing = 0.0
    for row in range(1, 14):
        spacing += row * 2.1
        y = horizon + spacing * SS
        if y > W:
            break
        shade = lerp(GRID_BRIGHT, GRID, min(1.0, spacing / 200.0))
        draw.line([(0, y), (W, y)], fill=shade, width=SS * 3)

    # Starfield above the horizon.
    stars = [(0.11, 0.13), (0.26, 0.07), (0.42, 0.18), (0.60, 0.06),
             (0.75, 0.15), (0.88, 0.10), (0.18, 0.28), (0.68, 0.31)]
    for sx, sy in stars:
        r = SS * (5 if (sx * 10) % 2 else 4)
        cx, cy = sx * W, sy * W
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(226, 234, 255))

    # Flat-shaded fighter, built from separately lit facets so it reads as
    # polygonal rather than as a flat sticker.
    def poly(points, colour):
        draw.polygon([(x * W, y * W) for x, y in points], fill=colour)

    # Wings, swept back from a heavier fuselage so the silhouette still holds
    # together when the whole icon is 60 pixels across.
    poly([(0.50, 0.42), (0.09, 0.72), (0.31, 0.75), (0.50, 0.64)], WING_DARK)
    poly([(0.50, 0.42), (0.91, 0.72), (0.69, 0.75), (0.50, 0.64)], WING_LIGHT)
    # Wing tips, canted up.
    poly([(0.09, 0.72), (0.05, 0.58), (0.23, 0.67)], WING_LIGHT)
    poly([(0.91, 0.72), (0.95, 0.58), (0.77, 0.67)], WING_DARK)
    # Fuselage: three facets meeting on the centreline.
    poly([(0.50, 0.22), (0.37, 0.72), (0.50, 0.80)], HULL_MID)
    poly([(0.50, 0.22), (0.63, 0.72), (0.50, 0.80)], HULL_LIGHT)
    poly([(0.37, 0.72), (0.63, 0.72), (0.50, 0.80)], HULL_DARK)
    # Canopy.
    poly([(0.50, 0.33), (0.435, 0.56), (0.50, 0.63), (0.565, 0.56)], (26, 36, 70))
    poly([(0.50, 0.33), (0.50, 0.63), (0.565, 0.56)], (54, 74, 128))
    # Engine: a flared plume rather than a ring, which read as a detached
    # donut once the icon was scaled down.
    poly([(0.435, 0.775), (0.565, 0.775), (0.545, 0.85), (0.455, 0.85)], ENGINE)
    poly([(0.462, 0.778), (0.538, 0.778), (0.523, 0.826), (0.477, 0.826)],
         ENGINE_CORE)

    image.resize((SIZE, SIZE), Image.LANCZOS).save(
        "src/app/ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png")
    print("wrote icon-1024.png")


if __name__ == "__main__":
    main()
