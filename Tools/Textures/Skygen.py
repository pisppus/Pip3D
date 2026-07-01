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


PANO_W            = 512
PANO_H            = 192
DEFAULT_SCREEN_W  = 480
DEFAULT_SCREEN_H  = 320
NOMINAL_HFOV_FRAC = 0.233
DEFAULT_COVERAGE  = 0.42
DEFAULT_SEED      = 0xC10D
DEFAULT_CLOUD     = "250,250,252"

PREVIEW_FILENAME  = "_CloudsMask.png"


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


def compute_repeats(screen_w):
    return max(1, round(float(screen_w) / (PANO_W * NOMINAL_HFOV_FRAC)))


SHADE_MIN = 0.72
SHADE_MAX = 1.07


def gen_cloud_mask(coverage, seed):
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
    t_h        = np.clip(v_norm / 0.92, 0.0, 1.0)
    horiz_fade = 1.0 - t_h * t_h * (3.0 - 2.0 * t_h)
    top_clear  = np.clip(v_norm * 9.0, 0.0, 1.0)
    alpha      = np.clip(alpha * horiz_fade * top_clear, 0.0, 1.0)

    ht    = 1.0 - v_norm
    inner = np.clip(alpha - 0.55, 0.0, 1.0) * 0.20
    shade = np.clip(0.78 + 0.22 * ht - inner, SHADE_MIN, SHADE_MAX)
    rim   = np.clip(1.0 - np.abs(alpha - 0.22) * 6.0, 0.0, 1.0) * 0.07
    shade = np.clip(shade + rim, SHADE_MIN, SHADE_MAX)

    alpha_plane = np.clip(np.rint(alpha * 255.0), 0, 255).astype(np.uint8)
    shade_plane = np.clip(np.rint((shade - SHADE_MIN) / (SHADE_MAX - SHADE_MIN) * 255.0), 0, 255).astype(np.uint8)
    return alpha_plane, shade_plane


def mask_to_preview(alpha_plane, shade_plane, cloud_rgb):
    a_norm = alpha_plane.astype(np.float32) / 255.0
    s_norm = SHADE_MIN + shade_plane.astype(np.float32) / 255.0 * (SHADE_MAX - SHADE_MIN)
    cr, cg, cb = cloud_rgb
    r = np.clip(cr * s_norm * a_norm, 0, 255)
    g = np.clip(cg * s_norm * a_norm, 0, 255)
    b = np.clip(cb * s_norm * a_norm, 0, 255)
    return np.stack([r, g, b], axis=-1).astype(np.uint8)

def packbits_encode_row(row, row_w):
    out = bytearray()
    i = 0
    n = len(row)
    while i < n:
        run = 1
        while i + run < n and row[i + run] == row[i] and run < 128:
            run += 1
        if run >= 2:
            out.append((run - 1) & 0x7F)
            out.append(row[i] & 0xFF)
            i += run
        else:
            lit_start = i
            i += 1
            while i < n and (i + 1 >= n or row[i] != row[i + 1]) and (i - lit_start) < 128:
                i += 1
            lit_len = i - lit_start
            out.append(0x80 | (lit_len - 1))
            out.extend(row[lit_start:lit_start + lit_len])
    return bytes(out)


def packbits_decode_row(data, off, row_w, dst):
    idx = off
    written = 0
    while written < row_w:
        c = data[idx]; idx += 1
        if c & 0x80:
            count = (c & 0x7F) + 1
            for _ in range(count):
                dst[written] = data[idx]; idx += 1; written += 1
        else:
            count = c + 1
            v = data[idx]; idx += 1
            for _ in range(count):
                dst[written] = v; written += 1
    return idx


def encode_plane(plane):
    flat = bytearray()
    offsets = np.zeros(PANO_H, dtype=np.uint32)
    scratch = bytearray(PANO_W)
    for y in range(PANO_H):
        offsets[y] = len(flat)
        row_bytes = plane[y].tobytes()
        enc = packbits_encode_row(row_bytes, PANO_W)
        flat.extend(enc)
        end = packbits_decode_row(enc, 0, PANO_W, scratch)
        assert end == len(enc), f"row {y}: decoder consumed {end} != {len(enc)}"
        assert bytes(scratch) == row_bytes, f"row {y}: round-trip mismatch"
    return bytes(flat), offsets


def write_mask_header(alpha_plane, shade_plane, cloud_rgb, repeats, out_path):
    a_flat, a_off = encode_plane(alpha_plane)
    s_flat, s_off = encode_plane(shade_plane)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    def fmt_row(b, per_line):
        lines = []
        for i in range(0, len(b), per_line):
            chunk = b[i:i + per_line]
            lines.append("            " + "".join(f"0x{v:02X}, " for v in chunk))
        return "\n".join(lines)

    def fmt_offsets(off):
        return "            " + "".join(f"{int(v)}, " for v in off) + "\n"

    cr, cg, cb = cloud_rgb
    default565 = to565(int(cr), int(cg), int(cb))
    total = len(a_flat) + len(s_flat) + 2 * PANO_H * 4

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("#pragma once\n")
        f.write("#include <cstdint>\n\n")
        f.write("namespace pip3D\n{\n")
        f.write("    namespace detail\n    {\n")
        f.write(f"        // PackBits-RLE cloud mask, two 8-bit planes (alpha, shade).\n")
        f.write(f"        // Run:    [count-1]            value   (count in 1..128)\n")
        f.write(f"        // Literal:[0x80|count-1]       bytes   (count in 1..128)\n")
        f.write(f"        alignas(16) static const uint8_t s_cloudsAlphaData[{len(a_flat)}] =\n        {{\n")
        f.write(fmt_row(a_flat, 24) + "\n")
        f.write("        };\n\n")
        f.write(f"        alignas(16) static const uint8_t s_cloudsShadeData[{len(s_flat)}] =\n        {{\n")
        f.write(fmt_row(s_flat, 24) + "\n")
        f.write("        };\n\n")
        f.write(f"        alignas(16) static const uint32_t s_cloudsAlphaOffset[{PANO_H}] =\n        {{\n")
        f.write(fmt_offsets(a_off))
        f.write("        };\n\n")
        f.write(f"        alignas(16) static const uint32_t s_cloudsShadeOffset[{PANO_H}] =\n        {{\n")
        f.write(fmt_offsets(s_off))
        f.write("        };\n    }\n\n")
        f.write(f"    inline constexpr const uint8_t*  g_cloudsAlpha       = detail::s_cloudsAlphaData;\n")
        f.write(f"    inline constexpr const uint8_t*  g_cloudsShade       = detail::s_cloudsShadeData;\n")
        f.write(f"    inline constexpr const uint32_t* g_cloudsAlphaOffset = detail::s_cloudsAlphaOffset;\n")
        f.write(f"    inline constexpr const uint32_t* g_cloudsShadeOffset = detail::s_cloudsShadeOffset;\n")
        f.write(f"    inline constexpr uint16_t        CLOUDS_PANO_W        = {PANO_W};\n")
        f.write(f"    inline constexpr uint16_t        CLOUDS_PANO_H        = {PANO_H};\n")
        f.write(f"    inline constexpr uint16_t        CLOUDS_PANO_REPEATS  = {repeats};\n")
        f.write(f"    inline constexpr uint16_t        CLOUDS_DEFAULT_COLOR = 0x{default565:04X};\n")
        f.write("}\n")
    return out_path, total


def main():
    p = argparse.ArgumentParser(description="pip3D cloud mask generator")
    p.add_argument("output",      nargs="?")
    p.add_argument("--coverage",  type=float, default=DEFAULT_COVERAGE)
    p.add_argument("--seed",      type=lambda x: int(x, 0), default=DEFAULT_SEED)
    p.add_argument("--cloud",     default=DEFAULT_CLOUD)
    p.add_argument("--screen-w",  type=int, default=DEFAULT_SCREEN_W)
    p.add_argument("--screen-h",  type=int, default=DEFAULT_SCREEN_H)
    args = p.parse_args()

    cloud_rgb = tuple(int(v) for v in args.cloud.split(","))
    repeats   = compute_repeats(args.screen_w)

    print(f"[Skygen] cloud mask: coverage={args.coverage}  seed={args.seed:#x}  cloud={cloud_rgb}")
    print(f"[Skygen] texture={PANO_W}x{PANO_H}  repeats={repeats}  (screen {args.screen_w}x{args.screen_h})")

    alpha_plane, shade_plane = gen_cloud_mask(args.coverage, args.seed)

    script_dir   = os.path.dirname(os.path.abspath(__file__))
    sources_dir  = os.path.join(script_dir, "Sources")
    os.makedirs(sources_dir, exist_ok=True)
    preview_path = os.path.join(sources_dir, PREVIEW_FILENAME)
    Image.fromarray(mask_to_preview(alpha_plane, shade_plane, cloud_rgb), "RGB").save(preview_path)
    print(f"[Skygen] Preview: {preview_path}")

    if args.output:
        hpp, total = write_mask_header(alpha_plane, shade_plane, cloud_rgb, repeats, args.output)
        print(f"[Skygen] Flash header: {hpp}  ({total} bytes total: two 8-bit RLE planes + offsets, 0 RAM)")


if __name__ == "__main__":
    main()