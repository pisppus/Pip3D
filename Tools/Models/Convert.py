import sys
sys.dont_write_bytecode = True

import struct
import os
import argparse
import math

if os.name == 'nt':
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)


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


def min_enclosing_ball(px, py, pz, iterations=128):
    n = len(px)
    if n == 0:
        return (0.0, 0.0, 0.0, 0.0)
    if n == 1:
        return (float(px[0]), float(py[0]), float(pz[0]), 0.0)

    min_x = max_x = float(px[0])
    min_y = max_y = float(py[0])
    min_z = max_z = float(pz[0])
    for i in range(1, n):
        xv = float(px[i]); yv = float(py[i]); zv = float(pz[i])
        if xv < min_x: min_x = xv
        elif xv > max_x: max_x = xv
        if yv < min_y: min_y = yv
        elif yv > max_y: max_y = yv
        if zv < min_z: min_z = zv
        elif zv > max_z: max_z = zv

    cx = (min_x + max_x) * 0.5
    cy = (min_y + max_y) * 0.5
    cz = (min_z + max_z) * 0.5

    fpx = [float(v) for v in px]
    fpy = [float(v) for v in py]
    fpz = [float(v) for v in pz]

    for k in range(1, iterations + 1):
        far_idx = 0
        max_d2 = -1.0
        for i in range(n):
            dx = fpx[i] - cx
            dy = fpy[i] - cy
            dz = fpz[i] - cz
            d2 = dx * dx + dy * dy + dz * dz
            if d2 > max_d2:
                max_d2 = d2
                far_idx = i

        step = 1.0 / k
        cx += (fpx[far_idx] - cx) * step
        cy += (fpy[far_idx] - cy) * step
        cz += (fpz[far_idx] - cz) * step

    max_d2 = 0.0
    for i in range(n):
        dx = fpx[i] - cx
        dy = fpy[i] - cy
        dz = fpz[i] - cz
        d2 = dx * dx + dy * dy + dz * dz
        if d2 > max_d2:
            max_d2 = d2

    return (cx, cy, cz, math.sqrt(max_d2))


def convert_obj2mesh(obj_path, force_output_path=None):
    if not os.path.exists(obj_path):
        print(f"\033[91m[-] Error: Source file {obj_path} not found!\033[0m")
        sys.exit(1)

    raw_vertices = []
    raw_normals = []
    raw_texcoords = []
    
    unique_verts = {}
    vertices = []
    faces = []
    has_uv = False

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
                elif parts[0] == 'vt':
                    raw_texcoords.append([float(parts[1]), float(parts[2])])
                    has_uv = True
                elif parts[0] == 'f':
                    face_corners = []
                    for p in parts[1:]:
                        sub_parts = p.split('/')
                        
                        v_idx = int(sub_parts[0])
                        v_idx = len(raw_vertices) + v_idx if v_idx < 0 else v_idx - 1
                        
                        vt_idx = -1
                        if len(sub_parts) >= 2 and sub_parts[1]:
                            vt_idx = int(sub_parts[1])
                            vt_idx = len(raw_texcoords) + vt_idx if vt_idx < 0 else vt_idx - 1
                        
                        vn_idx = -1
                        if len(sub_parts) >= 3 and sub_parts[2]:
                            vn_idx = int(sub_parts[2])
                            vn_idx = len(raw_normals) + vn_idx if vn_idx < 0 else vn_idx - 1
                        
                        face_corners.append((v_idx, vt_idx, vn_idx))

                    for k in range(1, len(face_corners) - 1):
                        tri = [face_corners[0], face_corners[k], face_corners[k+1]]
                        tri_indices = []
                        for corner in tri:
                            if corner not in unique_verts:
                                unique_verts[corner] = len(vertices)
                                pos = raw_vertices[corner[0]]
                                
                                if corner[2] != -1 and corner[2] < len(raw_normals):
                                    norm = raw_normals[corner[2]]
                                else:
                                    norm = [0.0, 1.0, 0.0]
                                    
                                if corner[1] != -1 and corner[1] < len(raw_texcoords):
                                    uv = raw_texcoords[corner[1]]
                                else:
                                    uv = [0.0, 0.0]
                                    
                                vertices.append({'pos': pos, 'norm': norm, 'uv': uv})
                            tri_indices.append(unique_verts[corner])
                        faces.append(tri_indices)
    except Exception as e:
        print(f"\033[91m[-] Error parsing OBJ file: {str(e)}\033[0m")
        sys.exit(1)

    if not vertices:
        print(f"\033[91m[-] Error: No valid polygons found in the file!\033[0m")
        sys.exit(1)

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

    bcx, bcy, bcz, br = min_enclosing_ball(px_list, py_list, pz_list, iterations=128)

    radius_ratio = br / 32767.0
    cx_ratio = bcx / 32767.0
    cy_ratio = bcy / 32767.0
    cz_ratio = bcz / 32767.0

    raw_name = os.path.splitext(os.path.basename(obj_path))[0]
    
    sanitized_name = raw_name.replace('-', '_').replace(' ', '_')
    class_name = sanitized_name[0].upper() + sanitized_name[1:] if sanitized_name else "Mesh"
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
                target_dir = os.path.join(test_path, "Models")
                break
            curr = os.path.dirname(curr)
            
        if target_dir:
            header_path = os.path.join(target_dir, raw_name + ".hpp")
        else:
            header_path = os.path.splitext(obj_path)[0] + ".hpp"
    
    vertex_bytes = verts_count * 16
    face_bytes = faces_count * 6
    total_bytes = vertex_bytes + face_bytes

    try:
        with open(header_path, 'w', encoding='utf-8') as out:
            out.write("/*\n")
            out.write(f" * Pip3D Model Asset — {class_name}\n")
            out.write(" * Generated automatically by Tools/Models/Convert.py. Do not edit.\n")
            out.write(" *\n")
            out.write(f" * Source File     : {os.path.basename(obj_path)}\n")
            out.write(f" * Vertices        : {verts_count} ({vertex_bytes} bytes)\n")
            out.write(f" * Triangles       : {faces_count} ({face_bytes} bytes)\n")
            out.write(f" * Has UVs         : {'Yes' if has_uv else 'No'}\n")
            out.write(f" * Bounding Sphere : Center({cx_ratio:.4f}, {cy_ratio:.4f}, {cz_ratio:.4f}), Radius({radius_ratio:.4f})\n")
            out.write(f" * Flash Memory    : {total_bytes} bytes ({total_bytes / 1024.0:.2f} KB)\n")
            out.write(" */\n\n")
            out.write("#pragma once\n\n")
            out.write("#include \"Geometry/Mesh.hpp\"\n\n")
            out.write("namespace pip3D\n{\n")
            out.write("    namespace detail\n    {\n")
            out.write(f"        alignas(16) static constexpr Vertex s_{var_name}Vertices[{verts_count}] = {{\n")
            for px, py, pz, v in zip(px_list, py_list, pz_list, vertices):
                n_packed = pack_normal(*v['norm'])
                tu, tv = v['uv'][0], v['uv'][1]
                tv_inverted = 1.0 - tv
                out.write(f"            {{ {px}, {py}, {pz}, {n_packed}, {tu:.6f}f, {tv_inverted:.6f}f }},\n")
            out.write("        };\n\n")
            out.write(f"        alignas(16) static constexpr Face s_{var_name}Faces[{faces_count}] = {{\n")
            for f in faces:
                out.write(f"            {{ {f[0]}, {f[1]}, {f[2]} }},\n")
            out.write("        };\n    }\n\n")
            
            out.write(f"    class {class_name} : public Mesh\n    {{\n    public:\n")
            out.write(f"        explicit {class_name}(float size = 1.0f)\n")
            out.write(f"            : Mesh(detail::s_{var_name}Vertices, {verts_count}, detail::s_{var_name}Faces, {faces_count}, true)\n")
            out.write("        {\n")
            out.write(f"            autoScale(size);\n")
            out.write(f"            finalizeGeometry({verts_count}, {faces_count},\n")
            out.write(f"                Vector3(size * 0.5f * ({cx_ratio:.8f}f),\n")
            out.write(f"                         size * 0.5f * ({cy_ratio:.8f}f),\n")
            out.write(f"                         size * 0.5f * ({cz_ratio:.8f}f)),\n")
            out.write(f"                size * 0.5f * ({radius_ratio:.8f}f));\n")
            out.write(f"            bindDeleter<{class_name}>();\n")
            out.write("        }\n    };\n}\n")
            
        rel_obj = os.path.join("Models", "Sources", os.path.basename(obj_path)).replace("\\", "/")
        rel_hpp = os.path.join("Geometry", "Models", os.path.basename(header_path)).replace("\\", "/")
        uv_status = "Yes" if has_uv else "No"
        print(f"\033[36m[Pip3D]\033[0m Converting: {rel_obj} -> {rel_hpp} ({verts_count} verts, {faces_count} tris, UV: {uv_status})")
        
    except Exception as e:
        print(f"\033[91m[-] Error exporting C++ header: {str(e)}\033[0m")
        sys.exit(1)

    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pip3D Wavefront OBJ -> C++ Header Static Mesh Exporter with UV support")
    parser.prog = "Convert"
    parser.add_argument("input", help="Path to the source .obj file")
    parser.add_argument("output", nargs="?", help="Optional path to the output .hpp file")
    
    args = parser.parse_args()
    convert_obj2mesh(args.input, args.output)