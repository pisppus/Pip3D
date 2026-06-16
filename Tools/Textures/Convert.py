#!/usr/bin/env python3
import os
import sys
import argparse
import math

try:
    from PIL import Image
except ImportError:
    print("[-] Ошибка: Библиотека Pillow не найдена. Она будет установлена мастер-скриптом.")
    sys.exit(1)

if os.name == 'nt':
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)

def convert_png2tex(img_path, force_output_path=None, target_size=None):
    if not os.path.exists(img_path):
        print(f"\033[91m[-] Ошибка: Исходное изображение {img_path} не найдено!\033[0m")
        sys.exit(1)

    try:
        img = Image.open(img_path).convert('RGB')
    except Exception as e:
        print(f"\033[91m[-] Ошибка открытия изображения: {str(e)}\033[0m")
        sys.exit(1)

    orig_width, orig_height = img.size

    try:
        resample_filter = Image.Resampling.LANCZOS
    except AttributeError:
        resample_filter = Image.LANCZOS if hasattr(Image, 'LANCZOS') else Image.ANTIALIAS

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
        img = img.resize((width, height), resample_filter)

    pixels = img.load()

    width_shift = int(math.log2(width))
    height_shift = int(math.log2(height))
    width_mask = width - 1
    height_mask = height - 1

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
                    r5 = (r >> 3) & 0x1F
                    g6 = (g >> 2) & 0x3F
                    b5 = (b >> 3) & 0x1F
                    rgb565 = (r5 << 11) | (g6 << 5) | b5
                    out.write(f"0x{rgb565:04X}, ")
                out.write("\n")
                
            out.write("        };\n    }\n\n")
            
            out.write(f"    inline Texture g_{var_name}Texture = {{\n")
            out.write(f"        .data = detail::s_{var_name}TextureData,\n")
            out.write(f"        .widthShift = {width_shift},\n")
            out.write(f"        .heightShift = {height_shift},\n")
            out.write(f"        .widthMask = {width_mask},\n")
            out.write(f"        .heightMask = {height_mask},\n")
            out.write(f"        .palette = nullptr\n")
            out.write("    };\n}\n")
            
        rel_img = os.path.join("Textures", "Sources", os.path.basename(img_path)).replace("\\", "/")
        rel_hpp = os.path.join("Rendering", "Display", "Textures", os.path.basename(header_path)).replace("\\", "/")
        flash_ram_bytes = width * height * 2
        print(f"\033[36m[Pip3D]\033[0m Converting: {rel_img} -> {rel_hpp} ({width}x{height} POT, {flash_ram_bytes / 1024.0:.2f} KB)")
        
    except Exception as e:
        print(f"\033[91m[-] Ошибка экспорта текстуры: {str(e)}\033[0m")
        sys.exit(1)

    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pip3D Image -> C++ RGB565 Texture Converter")
    parser.prog = "Convert"
    parser.add_argument("input", help="Путь к картинке")
    parser.add_argument("output", nargs="?", help="Путь к выходу .hpp")
    parser.add_argument("--size", type=int, help="Принудительный размер")
    
    args = parser.parse_args()
    convert_png2tex(args.input, args.output, (args.size if args.size else None))