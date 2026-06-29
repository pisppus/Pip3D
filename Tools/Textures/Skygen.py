#!/usr/bin/env python3

import sys
sys.dont_write_bytecode = True

import os
import argparse
import numpy as np

try:
    from PIL import Image
except ImportError:
    print("[-] Pillow not found: pip install Pillow")
    sys.exit(1)


SKY_PRESETS = {
    "day":      ((60, 140, 255),  (210, 230, 255), (110, 120, 140)),
    "sunset":   ((250, 130, 90),  (255, 210, 140), (80, 55, 100)),
    "night":    ((15, 40, 100),   (40, 90, 160),   (10, 25, 60)),
    "dawn":     ((120, 155, 230), (255, 195, 170), (90, 95, 120)),
    "overcast": ((140, 160, 175), (195, 205, 215), (95, 106, 106)),
    "midday":   ((180, 220, 255), (255, 255, 240), (130, 140, 120)),
    "storm":    ((50, 55, 65),    (80, 85, 95),    (30, 30, 35)),
    "space":    ((5, 0, 20),      (20, 10, 50),    (0, 0, 10)),
}

PANO_W            = 512
PANO_H            = 256
DEFAULT_SCREEN_W  = 480
DEFAULT_SCREEN_H  = 320
NOMINAL_HFOV_FRAC = 0.233
DEFAULT_PRESET    = "day"
DEFAULT_COVERAGE  = 0.42
DEFAULT_SEED      = 0xC10D
DEFAULT_CLOUD     = "250,250,252"
BN_TILE           = 64

PREVIEW_FILENAME  = "_Sky.png"


def _hash_grid(ix, iy, seed):
    h = (seed & 0xFFFF) ^ ((ix * 374761393) ^ (iy * 668265263))
    h ^= h >> 16
    h = (h * np.int64(0x7FEB352D)) & 0xFFFFFFFF
    h ^= h >> 15
    h = (h * np.int64(0x846CA68B)) & 0xFFFFFFFF
    h ^= h >> 16
    return h.astype(np.float32) * (1.0 / 4294967296.0)


def value_noise_2d(xs, ys, wrap_x, seed):
    x0  = np.floor(xs).astype(np.int32)
    y0  = np.floor(ys).astype(np.int32)
    tx  = (xs - x0).astype(np.float32)
    ty  = (ys - y0).astype(np.float32)
    u   = tx * tx * tx * (tx * (tx * 6 - 15) + 10)
    v   = ty * ty * ty * (ty * (ty * 6 - 15) + 10)
    x0w = x0 % wrap_x
    x1w = (x0 + 1) % wrap_x
    c00 = _hash_grid(x0w, y0,     seed)
    c10 = _hash_grid(x1w, y0,     seed)
    c01 = _hash_grid(x0w, y0 + 1, seed)
    c11 = _hash_grid(x1w, y0 + 1, seed)
    a   = c00 + (c10 - c00) * u
    b   = c01 + (c11 - c01) * u
    return a + (b - a) * v


def fbm2d(xs, ys, wrap_x, octaves, seed):
    d    = np.zeros_like(xs, dtype=np.float32)
    amp  = 0.5
    norm = 0.0
    for o in range(octaves):
        d   += value_noise_2d(xs, ys, wrap_x, seed ^ (o * 7919)) * amp
        norm += amp
        xs   = xs * 2.0
        ys   = ys * 2.0
        amp  *= 0.5
    return d / norm


def _worley_hash2(ix, iy, seed):
    ix  = ix.astype(np.int64)
    iy  = iy.astype(np.int64)
    s1  = np.int64(seed & 0xFFFF)
    h1  = s1 ^ (ix * np.int64(374761393)) ^ (iy * np.int64(668265263))
    h1  = h1 ^ ((h1 >> 16) & np.int64(0xFFFFFFFF))
    h1  = (h1 * np.int64(0x7FEB352D)) & np.int64(0xFFFFFFFF)
    h1  = h1 ^ ((h1 >> 15) & np.int64(0xFFFFFFFF))
    h1  = (h1 * np.int64(0x846CA68B)) & np.int64(0xFFFFFFFF)
    h1  = h1 ^ ((h1 >> 16) & np.int64(0xFFFFFFFF))
    ux  = (h1.astype(np.float64) * (1.0 / 4294967296.0)).astype(np.float32)
    s2  = np.int64((seed ^ 0xA5A5A5) & 0xFFFF)
    h2  = s2 ^ (ix * np.int64(60498839)) ^ (iy * np.int64(19583743))
    h2  = h2 ^ ((h2 >> 16) & np.int64(0xFFFFFFFF))
    h2  = (h2 * np.int64(0x5BD1E995)) & np.int64(0xFFFFFFFF)
    h2  = h2 ^ ((h2 >> 13) & np.int64(0xFFFFFFFF))
    uy  = (h2.astype(np.float64) * (1.0 / 4294967296.0)).astype(np.float32)
    return ux, uy


def worley_f1(xs, ys, nx_cells, seed):
    ix  = np.floor(xs).astype(np.int32)
    iy  = np.floor(ys).astype(np.int32)
    fx  = (xs - ix).astype(np.float32)
    fy  = (ys - iy).astype(np.float32)
    f1  = np.full(xs.shape, 1e9, dtype=np.float32)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            cxw      = (ix + dx) % nx_cells
            cy       = iy + dy
            ux, uy   = _worley_hash2(cxw, cy, seed)
            d2       = (fx - dx - ux) ** 2 + (fy - dy - uy) ** 2
            f1       = np.where(d2 < f1, d2, f1)
    return np.sqrt(f1)


def cloud_density(xs, ys, nx, seed):
    wx      = fbm2d(xs + 1.7, ys + 9.2, nx, 3, seed ^ 0xAABB) * 2.0 - 1.0
    wy      = fbm2d(xs + 8.3, ys + 2.8, nx, 3, seed ^ 0xCCDD) * 2.0 - 1.0
    xs2     = xs + wx * 0.6
    ys2     = ys + wy * 0.4
    cell_size = 0.50
    f1      = worley_f1(xs2, ys2, nx, seed)
    density = np.clip(1.0 - f1 / cell_size, 0.0, 1.0)
    detail  = fbm2d(xs2 * 3.0, ys2 * 3.0, nx, 4, seed ^ 0xF00D) * 0.35
    return np.clip(density + detail * density, 0.0, 1.0)


def to565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def _smoothstep_f(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def sky_color_row_float(top_rgb, horizon_rgb, ground_rgb, y, h):
    if y <= 0:
        return np.array(top_rgb, dtype=np.float32)
    if y >= h:
        return np.array(ground_rgb, dtype=np.float32)
    T = y / h
    if T < (166 / 256):
        s = _smoothstep_f(min(1.0, (T / (166 / 256)) * (395 / 256)))
        return np.array(top_rgb, dtype=np.float32) * (1 - s) + np.array(horizon_rgb, dtype=np.float32) * s
    else:
        gt = min(1.0, ((T - 166 / 256) / (1.0 - 166 / 256)) * (728 / 256))
        s  = _smoothstep_f(gt)
        return np.array(horizon_rgb, dtype=np.float32) * (1 - s) + np.array(ground_rgb, dtype=np.float32) * s


def compute_cloud_line_y(screen_h):
    return (int(screen_h) * 166 + 128) >> 8


def compute_repeats(screen_w):
    return max(1, round(float(screen_w) / (PANO_W * NOMINAL_HFOV_FRAC)))


def make_blue_noise_tile(size, seed):
    half  = size // 2
    rng   = np.random.default_rng(seed)
    n     = rng.random((half, half), dtype=np.float32) - 0.5
    f     = np.fft.fft2(n)
    fy    = np.fft.fftfreq(half).astype(np.float32)[:, None]
    fx    = np.fft.fftfreq(half).astype(np.float32)[None, :]
    r     = np.sqrt(fx * fx + fy * fy)
    sigma = 0.08
    f    *= 1.0 - np.exp(-(r * r) / (2.0 * sigma * sigma))
    base  = np.fft.ifft2(f).real.astype(np.float32)
    base -= base.mean()
    base /= np.max(np.abs(base)) + 1e-9
    top   = np.concatenate([base, base[:, ::-1]], axis=1)
    return np.concatenate([top, top[::-1, :]], axis=0)


def apply_blue_noise_dither(arr, seed):
    bn   = make_blue_noise_tile(BN_TILE, seed ^ 0xB1)
    ry   = (PANO_H + BN_TILE - 1) // BN_TILE
    rx   = (PANO_W + BN_TILE - 1) // BN_TILE
    tile = np.tile(bn, (ry, rx))[:PANO_H, :PANO_W]
    amp  = np.array([4.0, 2.0, 4.0], dtype=np.float32)
    return np.clip(arr + tile[:, :, None] * amp, 0.0, 255.0)


def gen_panorama(preset, coverage, seed, cloud_rgb, screen_h):
    top_rgb, horizon_rgb, ground_rgb = SKY_PRESETS[preset]
    cloud_line_y = compute_cloud_line_y(screen_h)

    nx     = 8
    ny     = 4
    xs_lin = np.linspace(0, nx, PANO_W, endpoint=False, dtype=np.float32)
    ys_lin = np.linspace(0, ny, PANO_H, endpoint=False, dtype=np.float32)
    xs, ys = np.meshgrid(xs_lin, ys_lin)

    density = cloud_density(xs, ys, nx, seed)

    cov_w   = coverage * 0.36
    d       = np.clip((density - cov_w) / (1.0 - cov_w), 0.0, 1.0)
    d_sharp = np.power(d, 0.6)
    alpha   = d_sharp * d_sharp * (3.0 - 2.0 * d_sharp)

    v_norm     = np.linspace(0.0, 1.0, PANO_H, dtype=np.float32)[:, None]
    t_h        = np.clip(v_norm / 0.75, 0.0, 1.0)
    horiz_fade = 1.0 - t_h * t_h * (3.0 - 2.0 * t_h)
    top_clear  = np.clip(v_norm * 6.0, 0.0, 1.0)
    alpha      = np.clip(alpha * horiz_fade * top_clear, 0.0, 1.0)

    ht    = 1.0 - v_norm
    inner = np.clip(alpha - 0.55, 0.0, 1.0) * 0.20
    shade = np.clip(0.78 + 0.22 * ht - inner, 0.72, 1.0)
    rim   = np.clip(1.0 - np.abs(alpha - 0.22) * 6.0, 0.0, 1.0) * 0.07
    shade = np.clip(shade + rim, 0.72, 1.07)

    cr, cg, cb = cloud_rgb

    sky_rows = np.zeros((PANO_H, 3), dtype=np.float32)
    for y in range(PANO_H):
        screen_y    = (y * cloud_line_y) // PANO_H
        sky_rows[y] = sky_color_row_float(top_rgb, horizon_rgb, ground_rgb, screen_y, screen_h)

    sky         = sky_rows[:, None, :]
    a3          = alpha[:, :, None]
    sh3         = shade[:, :, None]
    cloud_color = np.stack([
        np.full((PANO_H, PANO_W), cr, dtype=np.float32),
        np.full((PANO_H, PANO_W), cg, dtype=np.float32),
        np.full((PANO_H, PANO_W), cb, dtype=np.float32),
    ], axis=-1) * sh3
    cloud_color = np.minimum(cloud_color, 255.0)

    out = sky + (cloud_color - sky) * a3
    return out.astype(np.float32)


def write_header(arr, repeats, screen_w, screen_h, out_path):
    px = np.clip(arr, 0.0, 255.0)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("#pragma once\n")
        f.write("#include <cstdint>\n\n")
        f.write("namespace pip3D\n{\n")
        f.write("    namespace detail\n    {\n")
        f.write(f"        alignas(16) static const uint16_t s_cloudsPanoramaData[{PANO_W * PANO_H}] =\n        {{\n")
        for y in range(PANO_H):
            f.write("            ")
            for x in range(PANO_W):
                r = int(px[y, x, 0])
                g = int(px[y, x, 1])
                b = int(px[y, x, 2])
                f.write(f"0x{to565(r, g, b):04X}, ")
            f.write("\n")
        f.write("        };\n    }\n\n")
        f.write(f"    inline constexpr const uint16_t* g_cloudsPanorama    = detail::s_cloudsPanoramaData;\n")
        f.write(f"    inline constexpr uint16_t        CLOUDS_PANO_W       = {PANO_W};\n")
        f.write(f"    inline constexpr uint16_t        CLOUDS_PANO_H       = {PANO_H};\n")
        f.write(f"    inline constexpr uint16_t        CLOUDS_PANO_REPEATS = {repeats};\n")
        f.write("}\n")
    return out_path


def main():
    p = argparse.ArgumentParser(description="pip3D sky panorama generator")
    p.add_argument("output",      nargs="?")
    p.add_argument("--preset",    default=DEFAULT_PRESET, choices=list(SKY_PRESETS.keys()))
    p.add_argument("--coverage",  type=float, default=DEFAULT_COVERAGE)
    p.add_argument("--seed",      type=lambda x: int(x, 0), default=DEFAULT_SEED)
    p.add_argument("--cloud",     default=DEFAULT_CLOUD)
    p.add_argument("--screen-w",  type=int, default=DEFAULT_SCREEN_W)
    p.add_argument("--screen-h",  type=int, default=DEFAULT_SCREEN_H)
    p.add_argument("--list",      action="store_true")
    args = p.parse_args()

    if args.list:
        for name, (t, h, g) in SKY_PRESETS.items():
            print(f"  {name:10s}  top={t}  horizon={h}  ground={g}")
        return

    cloud_rgb = tuple(int(v) for v in args.cloud.split(","))
    repeats   = compute_repeats(args.screen_w)

    print(f"[Skygen] preset={args.preset}  coverage={args.coverage}  seed={args.seed:#x}")
    print(f"[Skygen] texture={PANO_W}x{PANO_H}  repeats={repeats}  (screen {args.screen_w}x{args.screen_h})")

    arr = gen_panorama(args.preset, args.coverage, args.seed, cloud_rgb, args.screen_h)

    script_dir   = os.path.dirname(os.path.abspath(__file__))
    sources_dir  = os.path.join(script_dir, "Sources")
    os.makedirs(sources_dir, exist_ok=True)
    preview_path = os.path.join(sources_dir, PREVIEW_FILENAME)
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB").save(preview_path)
    print(f"[Skygen] Preview: {preview_path}")

    arr = apply_blue_noise_dither(arr, args.seed)

    if args.output:
        hpp = write_header(arr, repeats, args.screen_w, args.screen_h, args.output)
        print(f"[Skygen] Flash header: {hpp}  ({PANO_W * PANO_H * 2} bytes, 0 RAM)")


if __name__ == "__main__":
    main()