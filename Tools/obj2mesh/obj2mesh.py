#!/usr/bin/env python3
import struct
import os
import sys
import argparse
import math

if os.name == 'nt':
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)

class Color:
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    BOLD = '\033[1m'
    END = '\033[0m'

def style(text, color_code):
    if sys.stdout.isatty():
        return f"{color_code}{text}{Color.END}"
    return text

def print_row(label, value, label_style=Color.BOLD, val_style=""):
    width = 46
    dots_count = width - len(label) - len(str(value))
    dots = "." * max(1, dots_count)
    styled_label = style(label, label_style)
    styled_val = style(str(value), val_style)
    print(f"│ {styled_label} {dots} {styled_val} │")

def pack_normal(nx, ny, nz):
    l1norm = abs(nx) + abs(ny) + abs(nz)
    if l1norm > 1e-6:
        inv_norm = 1.0 / l1norm
        nx *= inv_norm
        ny *= inv_norm
        if nz < 0.0:
            tx = nx
            nx = (1.0 - abs(ny)) * (1.0 if nx >= 0.0 else -1.0)
            ny = (1.0 - abs(tx)) * (1.0 if ny >= 0.0 else -1.0)
        px = int((nx * 0.5 + 0.5) * 255.0)
        py = int((ny * 0.5 + 0.5) * 255.0)
        return (px << 8) | py
    return 0

def convert_obj2mesh(obj_path, force_output_path=None):
    if not os.path.exists(obj_path):
        print(style(f"[-] Ошибка: Исходный файл {obj_path} не найден!", Color.RED))
        sys.exit(1)

    raw_vertices = []
    raw_normals = []
    
    unique_verts = {}
    vertices = []
    faces = []

    try:
        with open(obj_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                parts = line.split()
                if not parts:
                    continue

                if parts[0] == 'v':
                    raw_vertices.append([float(parts[1]), float(parts[2]), float(parts[3])])
                elif parts[0] == 'vn':
                    raw_normals.append([float(parts[1]), float(parts[2]), float(parts[3])])
                elif parts[0] == 'f':
                    face_corners = []
                    for p in parts[1:]:
                        sub_parts = p.split('/')
                        
                        v_idx = int(sub_parts[0])
                        if v_idx < 0:
                            v_idx = len(raw_vertices) + v_idx
                        else:
                            v_idx = v_idx - 1
                        
                        vn_idx = 0
                        if len(sub_parts) >= 3 and sub_parts[2]:
                            vn_idx = int(sub_parts[2])
                            if vn_idx < 0:
                                vn_idx = len(raw_normals) + vn_idx
                            else:
                                vn_idx = vn_idx - 1
                        else:
                            vn_idx = -1
                        
                        face_corners.append((v_idx, vn_idx))

                    for k in range(1, len(face_corners) - 1):
                        tri = [face_corners[0], face_corners[k], face_corners[k+1]]
                        tri_indices = []
                        for corner in tri:
                            if corner not in unique_verts:
                                unique_verts[corner] = len(vertices)
                                pos = raw_vertices[corner[0]]
                                if corner[1] != -1 and corner[1] < len(raw_normals):
                                    norm = raw_normals[corner[1]]
                                else:
                                    norm = [0.0, 1.0, 0.0]
                                vertices.append({'pos': pos, 'norm': norm})
                            tri_indices.append(unique_verts[corner])
                        faces.append(tri_indices)
    except Exception as e:
        print(style(f"[-] Ошибка при парсинге OBJ-файла: {str(e)}", Color.RED))
        sys.exit(1)

    if not vertices:
        print(style("[-] Ошибка: В файле не обнаружено валидных полигонов!", Color.RED))
        sys.exit(1)

    xs = [v[0] for v in raw_vertices]
    ys = [v[1] for v in raw_vertices]
    zs = [v[2] for v in raw_vertices]
    dim_x = max(xs) - min(xs)
    dim_y = max(ys) - min(ys)
    dim_z = max(zs) - min(zs)

    min_x = min(v['pos'][0] for v in vertices)
    max_x = max(v['pos'][0] for v in vertices)
    min_z = min(v['pos'][2] for v in vertices)
    max_z = max(v['pos'][2] for v in vertices)
    
    cx = (min_x + max_x) * 0.5
    cz = (min_z + max_z) * 0.5
    for v in vertices:
        v['pos'][0] -= cx
        v['pos'][2] -= cz

    max_val = max(max(abs(coord) for coord in v['pos']) for v in vertices)
    scale = 32767.0 / max_val if max_val > 1e-6 else 1.0

    verts_count = len(vertices)
    faces_count = len(faces)
    
    px_list = []
    py_list = []
    pz_list = []
    for v in vertices:
        px = int(v['pos'][0] * scale)
        py = int(v['pos'][1] * scale)
        pz = int(v['pos'][2] * scale)
        px_list.append(px)
        py_list.append(py)
        pz_list.append(pz)

    min_px, max_px = min(px_list), max(px_list)
    min_py, max_py = min(py_list), max(py_list)
    min_pz, max_pz = min(pz_list), max(pz_list)

    cx_int = (min_px + max_px) // 2
    cy_int = (min_py + max_py) // 2
    cz_int = (min_pz + max_pz) // 2

    max_dist_sq = 0
    for px, py, pz in zip(px_list, py_list, pz_list):
        dx = px - cx_int
        dy = py - cy_int
        dz = pz - cz_int
        dist_sq = dx*dx + dy*dy + dz*dz
        if dist_sq > max_dist_sq:
            max_dist_sq = dist_sq

    max_dist = math.sqrt(max_dist_sq)
    radius_ratio = max_dist / 32767.0
    cx_ratio = cx_int / 32767.0
    cy_ratio = cy_int / 32767.0
    cz_ratio = cz_int / 32767.0

    raw_name = os.path.splitext(os.path.basename(obj_path))[0]
    
    class_name = raw_name[0].upper() + raw_name[1:] if raw_name else "Mesh"
    var_name = class_name.lower()

    if force_output_path:
        header_path = force_output_path
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        target_dir = None
        curr = script_dir
        
        for _ in range(4):
            test_path = os.path.join(curr, "lib", "Pip3D", "Pip3D", "Geometry")
            if os.path.exists(test_path):
                target_dir = test_path
                break
            curr = os.path.dirname(curr)
            
        if target_dir:
            header_path = os.path.join(target_dir, raw_name + ".hpp")
        else:
            header_path = os.path.splitext(obj_path)[0] + ".hpp"
    
    try:
        with open(header_path, 'w', encoding='utf-8') as out:
            out.write("#pragma once\n#include \"Mesh.hpp\"\n\nnamespace pip3D\n{\n")
            out.write("    namespace detail\n    {\n")
            out.write(f"        alignas(16) static constexpr Vertex s_{var_name}Vertices[{verts_count}] = {{\n")
            for px, py, pz, v in zip(px_list, py_list, pz_list, vertices):
                n_packed = pack_normal(*v['norm'])
                out.write(f"            {{ {px}, {py}, {pz}, {n_packed} }},\n")
            out.write("        };\n\n")
            out.write(f"        alignas(16) static constexpr Face s_{var_name}Faces[{faces_count}] = {{\n")
            for f in faces:
                out.write(f"            {{ {f[0]}, {f[1]}, {f[2]} }},\n")
            out.write("        };\n    }\n\n")
            
            out.write(f"    class {class_name} : public Mesh\n    {{\n    public:\n")
            out.write(f"        {class_name}(float size = 1.0f, const Color &color = Color::WHITE)\n")
            out.write(f"            : Mesh(detail::s_{var_name}Vertices, {verts_count}, detail::s_{var_name}Faces, {faces_count}, color, true)\n")
            out.write("        {\n")
            out.write(f"            autoScale(size);\n")
            out.write(f"            cache.boundingCenter = Vector3(size * 0.5f * ({cx_ratio:.8f}f), size * 0.5f * ({cy_ratio:.8f}f), size * 0.5f * ({cz_ratio:.8f}f));\n")
            out.write(f"            cache.boundingRadius = size * 0.5f * ({radius_ratio:.8f}f);\n")
            out.write("            cache.boundsValid = true;\n")
            out.write("            cache.transform.identity();\n")
            out.write("            cache.maxScale = 1.0f;\n")
            out.write("            cache.transformValid = true;\n")
            out.write("            transformDirty = false;\n")
            out.write("            cache.transformHash = 4216742517u;\n")
            out.write("        }\n    };\n}\n")
        print(style(f"[+] Успешно сгенерирован и сохранен С++ заголовок: {header_path}", Color.GREEN + Color.BOLD))
    except Exception as e:
        print(style(f"[-] Ошибка при экспорте C++ заголовка: {str(e)}", Color.RED))
        sys.exit(1)

    mesh_geom_ram = (verts_count * 8) + (faces_count * 6)
    projection_cache_ram = verts_count * 24

    print("\n" + style("┌──────────────────────────────────────────────────┐", Color.CYAN))
    print("│ " + style(f"АНАЛИЗ И СТАТИЧЕСКИЙ ЭКСПОРТ: {os.path.basename(obj_path):<18}", Color.CYAN + Color.BOLD) + " │")
    print(style("├──────────────────────────────────────────────────┤", Color.CYAN))
    print_row("Вершин в исходном файле", len(raw_vertices))
    print_row("Вершин в сжатом файле", verts_count)
    print_row("Полигонов (треугольников)", faces_count)
    print_row("ОЗУ под геометрию (RAM)", "0.00 KB (Flash RODATA!)", Color.BOLD, Color.GREEN)
    print_row("ОЗУ под кэш проекций (RAM)", f"{(projection_cache_ram) / 1024.0:.2f} KB", Color.BOLD, Color.YELLOW)
    print_row("Ширина модели (X-axis)", f"{dim_x:.2f} units", Color.BOLD, Color.BLUE)
    print_row("Высота модели (Y-axis)", f"{dim_y:.2f} units", Color.BOLD, Color.BLUE)
    print_row("Глубина модели (Z-axis)", f"{dim_z:.2f} units", Color.BOLD, Color.BLUE)
    print(style("└──────────────────────────────────────────────────┘", Color.CYAN))

    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pip3D Wavefront OBJ -> C++ Header Static Mesh Exporter")
    parser.add_name = "obj2mesh"
    parser.add_argument("input", help="Путь к исходному .obj файлу")
    parser.add_argument("output", nargs="?", help="Опциональный путь к выходному .hpp файлу")
    
    args = parser.parse_args()
    convert_obj2mesh(args.input, args.output)