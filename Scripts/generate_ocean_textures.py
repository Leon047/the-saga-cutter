# The Saga Cutter - game jam project.
# Generates Docs/Art/Ocean/T_OceanPatterns.png (1024x1024 RGB, tiles in both axes).
#   R = Worley cell edges, 9x9 points (foam net)   G = tiling value noise, 3 octaves   B = Worley edges, 26x26 points
# Run: py Scripts/generate_ocean_textures.py
import os
import numpy as np
from PIL import Image

N = 1024
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Docs", "Art", "Ocean", "T_OceanPatterns.png")
rng = np.random.default_rng(20261115)

def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)

def worley_edges(n):
    """Wrap-around Worley on an n x n jittered grid; returns 1 - smoothstep(0, 0.12, F2 - F1), F in cell units."""
    pts = rng.random((n, n, 2))  # jitter inside each cell (x, y)
    ys, xs = np.mgrid[0:N, 0:N]
    u, v = xs * n / N, ys * n / N  # pixel position in cell units
    cx, cy = np.floor(u).astype(int), np.floor(v).astype(int)
    f1 = np.full((N, N), 1e9)
    f2 = np.full((N, N), 1e9)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            gx, gy = cx + dx, cy + dy
            p = pts[gy % n, gx % n]  # wrap the feature point lookup
            d = np.hypot(gx + p[..., 0] - u, gy + p[..., 1] - v)
            f2 = np.where(d < f1, f1, np.minimum(f2, d))
            f1 = np.minimum(f1, d)
    return 1.0 - smoothstep(0.0, 0.12, f2 - f1)

def value_noise(period):
    g = rng.random((period, period))
    ys, xs = np.mgrid[0:N, 0:N]
    u, v = xs * period / N, ys * period / N
    x0, y0 = np.floor(u).astype(int), np.floor(v).astype(int)
    fx, fy = u - x0, v - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = g[y0 % period, x0 % period], g[y0 % period, (x0 + 1) % period]
    c, d = g[(y0 + 1) % period, x0 % period], g[(y0 + 1) % period, (x0 + 1) % period]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy

R = worley_edges(9)
G = sum(value_noise(4 * 2 ** i) * 0.5 ** i for i in range(3))
G = (G - G.min()) / (G.max() - G.min())
B = worley_edges(26)

img = np.stack([R, G, B], axis=-1)
for c in range(3):  # seamless check: edge columns/rows vs their wrapped neighbours
    ch = img[..., c]
    assert np.abs(ch[:, 0] - ch[:, -1]).mean() < 0.08, "left/right seam"
    assert np.abs(ch[0, :] - ch[-1, :]).mean() < 0.08, "top/bottom seam"

os.makedirs(os.path.dirname(OUT), exist_ok=True)
Image.fromarray((img * 255 + 0.5).astype(np.uint8), "RGB").save(OUT)
print(os.path.abspath(OUT))
