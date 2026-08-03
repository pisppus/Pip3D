import sys
sys.dont_write_bytecode = True

import os
import argparse
import math

try:
    from PIL import Image
except ImportError:
    print("[-] Error: Pillow library not found. It will be installed by the master script.")
    sys.exit(1)

if os.name == 'nt':
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)


def _pixel_to_rgb565(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5


def _image_to_rgb565_array(img):
    pixels = img.load()
    w, h = img.size
    arr = []
    for y in range(h):
        for x in range(w):
            r, g, b = pixels[x, y]
            arr.append(_pixel_to_rgb565(r, g, b))
    return arr


def _write_hex_array_chunked(out, data, items_per_line=12, indent="            "):
    for i in range(0, len(data), items_per_line):
        chunk = data[i:i + items_per_line]
        hex_str = ", ".join(f"0x{v:04X}" for v in chunk)
        out.write(f"{indent}{hex_str},\n")


def prev_power_of_two(n):
    if n <= 1:
        return 1
    p = 1
    while (p << 1) <= n:
        p <<= 1
    return p


def convert_png2tex(img_path, force_output_path=None, target_size=None):
    if not os.path.exists(img_path):
        print(f"\033[91m[-] Error: Source image {img_path} not found!\033[0m")
        sys.exit(1)

    try:
        img = Image.open(img_path).convert('RGB')
    except Exception as e:
        print(f"\033[91m[-] Error opening image: {str(e)}\033[0m")
        sys.exit(1)

    orig_width, orig_height = img.size

    try:
        resample_lanczos = Image.Resampling.LANCZOS
    except AttributeError:
        resample_lanczos = Image.LANCZOS if hasattr(Image, "LANCZOS") else Image.ANTIALIAS

    try:
        resample_bilinear = Image.Resampling.BILINEAR
    except AttributeError:
        resample_bilinear = Image.BILINEAR if hasattr(Image, "BILINEAR") else Image.ANTIALIAS

    raw_name = os.path.splitext(os.path.basename(img_path))[0]
    name_parts = raw_name.split('_')
    clean_name = raw_name

    detected_size = target_size
    if len(name_parts) > 1 and not target_size:
        try:
            val = int(name_parts[-1])
            if val >= 16 and (val & (val - 1)) == 0:
                detected_size = val
                clean_name = "_".join(name_parts[:-1])
        except ValueError:
            pass

    src_square = min(orig_width, orig_height)
    if detected_size:
        tex_size = min(detected_size, src_square)
    else:
        tex_size = src_square

    tex_size = prev_power_of_two(tex_size)
    tex_size = max(16, min(128, tex_size))

    crop_size = min(orig_width, orig_height)
    left = (orig_width - crop_size) // 2
    top = (orig_height - crop_size) // 2
    img = img.crop((left, top, left + crop_size, top + crop_size))

    if img.size != (tex_size, tex_size):
        img = img.resize((tex_size, tex_size), resample_lanczos)

    base_array = _image_to_rgb565_array(img)
    width = height = tex_size
    shift = int(math.log2(tex_size))

    mip_levels = []
    mip_w, mip_h = tex_size, tex_size
    mip_img = img
    while mip_w > 1 or mip_h > 1:
        next_w = mip_w >> 1
        next_h = mip_h >> 1
        if next_w < 1: next_w = 1
        if next_h < 1: next_h = 1
        mip_img = mip_img.resize((next_w, next_h), resample_bilinear)
        mip_levels.append((next_w, next_h, _image_to_rgb565_array(mip_img)))
        mip_w, mip_h = next_w, next_h

    mip_count = len(mip_levels)

    var_name = clean_name.lower()

    if force_output_path:
        header_path = force_output_path
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        target_dir = None
        curr = script_dir
        for _ in range(4):
            test_path = os.path.join(curr, "lib", "Pip3D", "Pip3D", "Rendering", "Display")
            if os.path.exists(test_path):
                target_dir = os.path.join(test_path, "Textures")
                break
            curr = os.path.dirname(curr)

        if target_dir:
            header_path = os.path.join(target_dir, clean_name + ".hpp")
        else:
            header_path = os.path.splitext(img_path)[0] + ".hpp"

    base_bytes = width * height * 2
    mip_bytes = sum(len(arr) * 2 for _, _, arr in mip_levels)
    total_bytes = base_bytes + mip_bytes

    try:
        with open(header_path, 'w', encoding='utf-8') as out:
            out.write("/*\n")
            out.write(f" * Pip3D Texture Asset — {clean_name}\n")
            out.write(" * Generated automatically by Tools/Textures/Convert.py. Do not edit.\n")
            out.write(" *\n")
            out.write(f" * Source File   : {os.path.basename(img_path)} ({orig_width}x{orig_height})\n")
            out.write(f" * Texture Size : {width}x{height} (square, RGB565)\n")
            out.write(f" * Mipmaps      : {mip_count} level(s)\n")
            out.write(f" * Flash Memory : {base_bytes} bytes base + {mip_bytes} bytes mips = {total_bytes} bytes ({total_bytes / 1024.0:.2f} KB)\n")
            out.write(" */\n\n")
            out.write("#pragma once\n\n")
            out.write("#include \"Rendering/Display/Texture.hpp\"\n\n")
            out.write("namespace pip3D\n{\n")
            out.write("    namespace detail\n    {\n")
            out.write(f"        // Base Level (LOD 0): {width}x{height}\n")
            out.write(f"        alignas(16) static const uint16_t s_{var_name}TextureData[{width * height}] = {{\n")
            _write_hex_array_chunked(out, base_array, items_per_line=12)
            out.write("        };\n\n")

            if mip_count > 0:
                total_mip_pixels = sum(len(arr) for _, _, arr in mip_levels)
                out.write(f"        // Mipmap Levels (LOD 1 .. LOD {mip_count}): {total_mip_pixels} total pixels\n")
                out.write(f"        alignas(16) static const uint16_t s_{var_name}MipData[{total_mip_pixels}] = {{\n")
                for level_idx, (mw, mh, arr) in enumerate(mip_levels):
                    out.write(f"            // LOD {level_idx + 1}: {mw}x{mh}\n")
                    _write_hex_array_chunked(out, arr, items_per_line=12)
                out.write("        };\n\n")

            out.write("    }\n\n")

            out.write(f"    inline Texture g_{var_name}Texture = {{\n")
            out.write(f"        .data     = detail::s_{var_name}TextureData,\n")
            if mip_count > 0:
                out.write(f"        .mipData  = detail::s_{var_name}MipData,\n")
            else:
                out.write(f"        .mipData  = nullptr,\n")
            out.write(f"        .shift    = {shift},\n")
            out.write(f"        .mipCount = {mip_count}\n")
            out.write("    };\n}\n")

        rel_img = os.path.join("Textures", "Sources", os.path.basename(img_path)).replace("\\", "/")
        rel_hpp = os.path.join("Rendering", "Display", "Textures", os.path.basename(header_path)).replace("\\", "/")
        print(f"\033[36m[Pip3D]\033[0m Converting: {rel_img} -> {rel_hpp} ({width}x{height} square POT, {base_bytes / 1024.0:.2f} KB + {mip_count} mips = {mip_bytes / 1024.0:.2f} KB, {total_bytes / 1024.0:.2f} KB total)")

    except Exception as e:
        print(f"\033[91m[-] Error exporting texture: {str(e)}\033[0m")
        sys.exit(1)

    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pip3D Image -> C++ RGB565 Texture Converter")
    parser.prog = "Convert"
    parser.add_argument("input", help="Path to the input image")
    parser.add_argument("output", nargs="?", help="Path to the output .hpp file")
    parser.add_argument("--size", type=int, help="Force specific (square) texture size")

    args = parser.parse_args()
    convert_png2tex(args.input, args.output, (args.size if args.size else None))