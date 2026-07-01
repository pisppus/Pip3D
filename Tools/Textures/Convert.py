#!/usr/bin/env python3
import os
import sys
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
        resample_lanczos = Image.LANCZOS if hasattr(Image, 'LANCZOS') else Image.ANTIALIAS

    try:
        resample_bilinear = Image.Resampling.BILINEAR
    except AttributeError:
        resample_bilinear = Image.BILINEAR if hasattr(Image, 'BILINEAR') else Image.ANTIALIAS

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

    def prev_power_of_two(n):
        if n <= 0: return 1
        return 1 << (n).bit_length() - 1

    if detected_size:
        width = min(detected_size, orig_width)
        height = min(detected_size, orig_height)
        width = prev_power_of_two(width)
        height = prev_power_of_two(height)
    else:
        width = prev_power_of_two(orig_width)
        height = prev_power_of_two(orig_height)

    width = min(max(width, 16), 128)
    height = min(max(height, 16), 128)

    if (width != orig_width) or (height != orig_height):
        img = img.resize((width, height), resample_lanczos)

    pixels = img.load()

    width_shift = int(math.log2(width))
    height_shift = int(math.log2(height))
    width_mask = width - 1
    height_mask = height - 1

    mip_levels = []
    mip_w, mip_h = width, height
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

    class_name = clean_name[0].upper() + clean_name[1:] if clean_name else "Texture"
    var_name = class_name.lower()

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

    try:
        with open(header_path, 'w', encoding='utf-8') as out:
            out.write("#pragma once\n#include \"Rendering/Display/Texture.hpp\"\n\nnamespace pip3D\n{\n")
            out.write("    namespace detail\n    {\n")
            out.write(f"        alignas(16) static const uint16_t s_{var_name}TextureData[{width * height}] = {{\n")
            for y in range(height):
                out.write("            ")
                for x in range(width):
                    r, g, b = pixels[x, y]
                    rgb565 = _pixel_to_rgb565(r, g, b)
                    out.write(f"0x{rgb565:04X}, ")
                out.write("\n")
            out.write("        };\n\n")

            if mip_count > 0:
                total_mip_pixels = sum(len(arr) for _, _, arr in mip_levels)
                out.write(f"        alignas(16) static const uint16_t s_{var_name}MipData[{total_mip_pixels}] = {{\n")
                for level_idx, (mw, mh, arr) in enumerate(mip_levels):
                    out.write(f"            // mip level {level_idx + 1}: {mw}x{mh}\n")
                    for y in range(mh):
                        out.write("            ")
                        for x in range(mw):
                            out.write(f"0x{arr[y * mw + x]:04X}, ")
                        out.write("\n")
                out.write("        };\n\n")

            out.write("    }\n\n")

            out.write(f"    inline Texture g_{var_name}Texture = {{\n")
            out.write(f"        .data = detail::s_{var_name}TextureData,\n")
            out.write(f"        .widthShift = {width_shift},\n")
            out.write(f"        .heightShift = {height_shift},\n")
            out.write(f"        .widthMask = {width_mask},\n")
            out.write(f"        .heightMask = {height_mask},\n")
            out.write(f"        .palette = nullptr,\n")
            if mip_count > 0:
                out.write(f"        .mipData = detail::s_{var_name}MipData,\n")
            else:
                out.write(f"        .mipData = nullptr,\n")
            out.write(f"        .mipCount = {mip_count}\n")
            out.write("    };\n}\n")

        rel_img = os.path.join("Textures", "Sources", os.path.basename(img_path)).replace("\\", "/")
        rel_hpp = os.path.join("Rendering", "Display", "Textures", os.path.basename(header_path)).replace("\\", "/")
        base_bytes = width * height * 2
        mip_bytes = sum(len(arr) * 2 for _, _, arr in mip_levels)
        total_bytes = base_bytes + mip_bytes
        print(f"\033[36m[Pip3D]\033[0m Converting: {rel_img} -> {rel_hpp} ({width}x{height} POT, {base_bytes / 1024.0:.2f} KB + {mip_count} mips = {mip_bytes / 1024.0:.2f} KB, {total_bytes / 1024.0:.2f} KB total)")

    except Exception as e:
        print(f"\033[91m[-] Error exporting texture: {str(e)}\033[0m")
        sys.exit(1)

    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pip3D Image -> C++ RGB565 Texture Converter")
    parser.prog = "Convert"
    parser.add_argument("input", help="Path to the input image")
    parser.add_argument("output", nargs="?", help="Path to the output .hpp file")
    parser.add_argument("--size", type=int, help="Force specific texture size")
    
    args = parser.parse_args()
    convert_png2tex(args.input, args.output, (args.size if args.size else None))