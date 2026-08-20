import sys
sys.dont_write_bytecode = True

import os
import argparse
import math

if os.name == 'nt':
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)


def _tag(msg):
    return f"\033[36m[Pip3D]\033[0m {msg}"


def _err(msg):
    return f"\033[91m[-] Error: {msg}\033[0m"


TILE_DEFAULT_CHUNK_SIZE = 32
CHUNK_SUFFIX = "_chunk"


def pack_normal(nx, ny, nz):
    l1norm = abs(nx) + abs(ny) + abs(nz)
    if l1norm > 1e-6:
        inv_norm = 1.0 / l1norm
        ox = nx * inv_norm
        oy = ny * inv_norm
        if nz < 0.0:
            sgn_x = 1.0 if ox >= 0.0 else -1.0
            sgn_y = 1.0 if oy >= 0.0 else -1.0
            ax = abs(ox)
            ay = abs(oy)
            ox = (1.0 - ay) * sgn_x
            oy = (1.0 - ax) * sgn_y
        px = int(round(ox * 127.5 + 127.5))
        py = int(round(oy * 127.5 + 127.5))
        px = max(0, min(255, px))
        py = max(0, min(255, py))
        return (px << 8) | py
    return 0


def compute_face_normal(p0, p1, p2):
    ax = p1[0] - p0[0]; ay = p1[1] - p0[1]; az = p1[2] - p0[2]
    bx = p2[0] - p0[0]; by = p2[1] - p0[1]; bz = p2[2] - p0[2]
    nx = ay * bz - az * by
    ny = az * bx - ax * bz
    nz = ax * by - ay * bx
    len_sq = nx * nx + ny * ny + nz * nz
    if len_sq > 1e-12:
        inv_len = 1.0 / math.sqrt(len_sq)
        return [nx * inv_len, ny * inv_len, nz * inv_len]
    return [0.0, 1.0, 0.0]


def dist_sq(p0, p1):
    dx = p0[0] - p1[0]
    dy = p0[1] - p1[1]
    dz = p0[2] - p1[2]
    return dx * dx + dy * dy + dz * dz


def min_enclosing_ball(px, py, pz, iterations=128):
    n = len(px)
    if n == 0:
        return (0.0, 0.0, 0.0, 0.0)
    cx = float(px[0])
    cy = float(py[0])
    cz = float(pz[0])
    radius_sq = 0.0
    max_d2 = 0.0
    for _ in range(iterations):
        max_d2 = 0.0
        max_i = 0
        for i in range(n):
            dx = px[i] - cx
            dy = py[i] - cy
            dz = pz[i] - cz
            d2 = dx * dx + dy * dy + dz * dz
            if d2 > max_d2:
                max_d2 = d2
                max_i = i
        if max_d2 <= radius_sq + 1e-6:
            break
        if radius_sq == 0.0:
            alpha = 1.0
        else:
            alpha = max_d2 / (max_d2 + radius_sq)
        cx = cx + alpha * (px[max_i] - cx)
        cy = cy + alpha * (py[max_i] - cy)
        cz = cz + alpha * (pz[max_i] - cz)
        radius_sq = max_d2
    return (cx, cy, cz, math.sqrt(max_d2))


def cluster_triangles_kd(tri_list, max_chunk_size=TILE_DEFAULT_CHUNK_SIZE):
    chunks = []

    def bisect(subset):
        if len(subset) <= max_chunk_size:
            chunks.append(subset)
            return

        min_x = min(t['cx'] for t in subset); max_x = max(t['cx'] for t in subset)
        min_y = min(t['cy'] for t in subset); max_y = max(t['cy'] for t in subset)
        min_z = min(t['cz'] for t in subset); max_z = max(t['cz'] for t in subset)

        dx = max_x - min_x
        dy = max_y - min_y
        dz = max_z - min_z

        if dx >= dy and dx >= dz:
            axis = 'cx'
        elif dy >= dz:
            axis = 'cy'
        else:
            axis = 'cz'

        subset.sort(key=lambda t: t[axis])
        mid = len(subset) // 2
        bisect(subset[:mid])
        bisect(subset[mid:])

    bisect(tri_list)
    return chunks


def compute_chunk_cone(tri_list, raw_verts):
    sum_nx = 0.0
    sum_ny = 0.0
    sum_nz = 0.0
    face_normals = []

    for tri in tri_list:
        p0 = raw_verts[tri['corners'][0][0]]
        p1 = raw_verts[tri['corners'][1][0]]
        p2 = raw_verts[tri['corners'][2][0]]
        n = compute_face_normal(p0, p1, p2)
        face_normals.append(n)
        sum_nx += n[0]
        sum_ny += n[1]
        sum_nz += n[2]

    inv_len = 1.0 / max(1e-12, math.sqrt(sum_nx * sum_nx + sum_ny * sum_ny + sum_nz * sum_nz))
    avg_nx = sum_nx * inv_len
    avg_ny = sum_ny * inv_len
    avg_nz = sum_nz * inv_len

    min_dot = 1.0
    for fn in face_normals:
        dot = fn[0] * avg_nx + fn[1] * avg_ny + fn[2] * avg_nz
        if dot < min_dot:
            min_dot = dot
    min_dot = max(-1.0, min(1.0, min_dot))

    qnormX = max(-32767, min(32767, int(round(avg_nx * 32767.0))))
    qnormY = max(-32767, min(32767, int(round(avg_ny * 32767.0))))
    qnormZ = max(-32767, min(32767, int(round(avg_nz * 32767.0))))

    if min_dot <= 0.0:
        qcone = 0
    else:
        sin_half = math.sqrt(max(0.0, 1.0 - min_dot * min_dot))
        qcone = max(1, min(32767, int(round(sin_half * 32767.0))))

    return qnormX, qnormY, qnormZ, qcone


def parse_obj(obj_path):
    raw_vertices = []
    raw_normals = []
    raw_texcoords = []
    parsed_triangles = []
    has_uv = False

    try:
        with open(obj_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                parts = line.strip().split()
                if not parts:
                    continue

                token = parts[0]
                if token == 'v':
                    raw_vertices.append([float(parts[1]), float(parts[2]), float(parts[3])])
                elif token == 'vn':
                    raw_normals.append([float(parts[1]), float(parts[2]), float(parts[3])])
                elif token == 'vt':
                    raw_texcoords.append([float(parts[1]), float(parts[2])])
                    has_uv = True
                elif token == 'f':
                    face_corners = []
                    for p in parts[1:]:
                        sub = p.split('/')
                        v_idx = int(sub[0])
                        v_idx = len(raw_vertices) + v_idx if v_idx < 0 else v_idx - 1

                        vt_idx = -1
                        if len(sub) >= 2 and sub[1]:
                            vt_idx = int(sub[1])
                            vt_idx = len(raw_texcoords) + vt_idx if vt_idx < 0 else vt_idx - 1

                        vn_idx = -1
                        if len(sub) >= 3 and sub[2]:
                            vn_idx = int(sub[2])
                            vn_idx = len(raw_normals) + vn_idx if vn_idx < 0 else vn_idx - 1

                        face_corners.append((v_idx, vt_idx, vn_idx))

                    num_corners = len(face_corners)
                    if num_corners == 3:
                        triangles = [face_corners]
                    elif num_corners == 4:
                        p0 = raw_vertices[face_corners[0][0]]
                        p1 = raw_vertices[face_corners[1][0]]
                        p2 = raw_vertices[face_corners[2][0]]
                        p3 = raw_vertices[face_corners[3][0]]
                        if dist_sq(p0, p2) <= dist_sq(p1, p3):
                            triangles = [
                                [face_corners[0], face_corners[1], face_corners[2]],
                                [face_corners[0], face_corners[2], face_corners[3]]
                            ]
                        else:
                            triangles = [
                                [face_corners[0], face_corners[1], face_corners[3]],
                                [face_corners[1], face_corners[2], face_corners[3]]
                            ]
                    else:
                        triangles = [
                            [face_corners[0], face_corners[k], face_corners[k + 1]]
                            for k in range(1, num_corners - 1)
                        ]

                    for tri in triangles:
                        p0 = raw_vertices[tri[0][0]]
                        p1 = raw_vertices[tri[1][0]]
                        p2 = raw_vertices[tri[2][0]]
                        fallback_norm = compute_face_normal(p0, p1, p2)

                        parsed_triangles.append({
                            'corners': tri,
                            'fallback_norm': fallback_norm,
                            'cx': (p0[0] + p1[0] + p2[0]) / 3.0,
                            'cy': (p0[1] + p1[1] + p2[1]) / 3.0,
                            'cz': (p0[2] + p1[2] + p2[2]) / 3.0
                        })
    except Exception as e:
        print(_err(f"Parsing OBJ: {str(e)}"))
        sys.exit(1)

    return raw_vertices, raw_normals, raw_texcoords, parsed_triangles, has_uv


def build_flat_mesh(parsed_triangles, raw_vertices, raw_normals, raw_texcoords, center_y):
    min_x = min(v[0] for v in raw_vertices); max_x = max(v[0] for v in raw_vertices)
    min_y = min(v[1] for v in raw_vertices); max_y = max(v[1] for v in raw_vertices)
    min_z = min(v[2] for v in raw_vertices); max_z = max(v[2] for v in raw_vertices)

    cx = (min_x + max_x) * 0.5
    cz = (min_z + max_z) * 0.5
    cy = (min_y + max_y) * 0.5 if center_y else 0.0

    centered_raw_verts = [
        [v[0] - cx, v[1] - cy, v[2] - cz]
        for v in raw_vertices
    ]

    max_val = max(max(abs(v[0]), abs(v[1]), abs(v[2])) for v in centered_raw_verts)
    scale = 32767.0 / max_val if max_val > 1e-6 else 1.0

    ordered_vertices = []
    ordered_faces = []
    unique_verts = {}

    for tri in parsed_triangles:
        tri_indices = []
        for corner in tri['corners']:
            if corner not in unique_verts:
                unique_verts[corner] = len(ordered_vertices)

                pos = centered_raw_verts[corner[0]]
                px = max(-32767, min(32767, int(round(pos[0] * scale))))
                py = max(-32767, min(32767, int(round(pos[1] * scale))))
                pz = max(-32767, min(32767, int(round(pos[2] * scale))))

                if corner[2] != -1 and corner[2] < len(raw_normals):
                    norm = raw_normals[corner[2]]
                else:
                    norm = tri['fallback_norm']

                if corner[1] != -1 and corner[1] < len(raw_texcoords):
                    uv = raw_texcoords[corner[1]]
                else:
                    uv = [0.0, 0.0]

                ordered_vertices.append({'px': px, 'py': py, 'pz': pz, 'norm': norm, 'uv': uv})
            tri_indices.append(unique_verts[corner])
        ordered_faces.append(tri_indices)

    return ordered_vertices, ordered_faces


def build_clustered_mesh(parsed_triangles, raw_vertices, raw_normals, raw_texcoords, center_y, chunk_size):
    min_x = min(v[0] for v in raw_vertices); max_x = max(v[0] for v in raw_vertices)
    min_y = min(v[1] for v in raw_vertices); max_y = max(v[1] for v in raw_vertices)
    min_z = min(v[2] for v in raw_vertices); max_z = max(v[2] for v in raw_vertices)

    cx = (min_x + max_x) * 0.5
    cz = (min_z + max_z) * 0.5
    cy = (min_y + max_y) * 0.5 if center_y else 0.0

    centered_raw_verts = [
        [v[0] - cx, v[1] - cy, v[2] - cz]
        for v in raw_vertices
    ]

    max_val = max(max(abs(v[0]), abs(v[1]), abs(v[2])) for v in centered_raw_verts)
    scale = 32767.0 / max_val if max_val > 1e-6 else 1.0

    clustered_chunks = cluster_triangles_kd(parsed_triangles, max_chunk_size=chunk_size)

    ordered_vertices = []
    ordered_faces = []
    chunk_headers = []

    for ch in clustered_chunks:
        chunk_v_offset = len(ordered_vertices)
        chunk_f_offset = len(ordered_faces)

        chunk_unique_verts = {}
        ch_min_x = 32767; ch_max_x = -32767
        ch_min_y = 32767; ch_max_y = -32767
        ch_min_z = 32767; ch_max_z = -32767

        for tri in ch:
            tri_indices = []
            for corner in tri['corners']:
                if corner not in chunk_unique_verts:
                    local_idx = len(chunk_unique_verts)
                    chunk_unique_verts[corner] = local_idx

                    pos = centered_raw_verts[corner[0]]
                    px = max(-32767, min(32767, int(round(pos[0] * scale))))
                    py = max(-32767, min(32767, int(round(pos[1] * scale))))
                    pz = max(-32767, min(32767, int(round(pos[2] * scale))))

                    if corner[2] != -1 and corner[2] < len(raw_normals):
                        norm = raw_normals[corner[2]]
                    else:
                        norm = tri['fallback_norm']

                    if corner[1] != -1 and corner[1] < len(raw_texcoords):
                        uv = raw_texcoords[corner[1]]
                    else:
                        uv = [0.0, 0.0]

                    ordered_vertices.append({'px': px, 'py': py, 'pz': pz, 'norm': norm, 'uv': uv})

                    if px < ch_min_x: ch_min_x = px
                    if px > ch_max_x: ch_max_x = px
                    if py < ch_min_y: ch_min_y = py
                    if py > ch_max_y: ch_max_y = py
                    if pz < ch_min_z: ch_min_z = pz
                    if pz > ch_max_z: ch_max_z = pz

                tri_indices.append(chunk_unique_verts[corner])

            ordered_faces.append(tri_indices)

        qnormX, qnormY, qnormZ, qcone = compute_chunk_cone(ch, centered_raw_verts)

        chunk_headers.append({
            'minX': ch_min_x, 'minY': ch_min_y, 'minZ': ch_min_z,
            'maxX': ch_max_x, 'maxY': ch_max_y, 'maxZ': ch_max_z,
            'vOffset': chunk_v_offset,
            'faceOffset': chunk_f_offset,
            'vCount': len(chunk_unique_verts),
            'faceCount': len(ch),
            'normX': qnormX, 'normY': qnormY, 'normZ': qnormZ,
            'coneDot': qcone,
        })

    return ordered_vertices, ordered_faces, chunk_headers, scale, centered_raw_verts


def write_header(out_path, class_name, var_name,
                 ordered_vertices, ordered_faces, chunk_headers,
                 bcx, bcy, bcz, br, use_face32,
                 meshlet_mode, src_basename):
    verts_count = len(ordered_vertices)
    faces_count = len(ordered_faces)
    chunks_count = len(chunk_headers) if chunk_headers else 0

    radius_ratio = br / 32767.0
    cx_ratio = bcx / 32767.0
    cy_ratio = bcy / 32767.0
    cz_ratio = bcz / 32767.0

    vertex_bytes = verts_count * 16
    face_bytes = faces_count * (12 if use_face32 else 6)
    chunk_bytes = chunks_count * 32
    total_bytes = vertex_bytes + face_bytes + chunk_bytes

    face_typename = "Face32" if use_face32 else "Face"
    face_ctor_suffix = "u" if use_face32 else ""

    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("/*\n")
        f.write(f" * Pip3D Asset — {class_name}\n")
        f.write(" * Generated automatically by Tools/Models/Convert.py. Do not edit.\n")
        f.write(" *\n")
        f.write(f" * Source File     : {src_basename}\n")
        if meshlet_mode:
            f.write(f" * Mode            : Meshlet (KD-tree spatial clustering)\n")
            f.write(f" * Spatial Chunks  : {chunks_count} ({chunk_bytes} bytes)\n")
        else:
            f.write(f" * Mode            : Flat mesh (no chunks)\n")
        f.write(f" * Vertices        : {verts_count} ({vertex_bytes} bytes, 16B/vertex)\n")
        f.write(f" * Triangles       : {faces_count} ({face_bytes} bytes, {12 if use_face32 else 6}B/tri, {face_typename})\n")
        f.write(f" * Bounding Sphere : Center({cx_ratio:.4f}, {cy_ratio:.4f}, {cz_ratio:.4f}), Radius({radius_ratio:.4f})\n")
        f.write(f" * Flash Memory    : {total_bytes} bytes ({total_bytes / 1024.0:.2f} KB)\n")
        f.write(" */\n\n")

        f.write("#pragma once\n\n")
        f.write('#include "Geometry/Mesh.hpp"\n\n')
        f.write("namespace pip3D\n{\n")
        f.write("    namespace detail\n    {\n")

        f.write(f"        alignas(16) static constexpr Vertex s_{var_name}Vertices[{verts_count}] = {{\n")
        for v in ordered_vertices:
            n_packed = pack_normal(*v['norm'])
            tu, tv = v['uv'][0], v['uv'][1]
            tv_inverted = 1.0 - tv
            f.write(f"            {{ {v['px']}, {v['py']}, {v['pz']}, {n_packed}, {tu:.6f}f, {tv_inverted:.6f}f }},\n")
        f.write("        };\n\n")

        f.write(f"        alignas(16) static constexpr {face_typename} s_{var_name}Faces[{faces_count}] = {{\n")
        for face in ordered_faces:
            f.write(f"            {{ {face[0]}{face_ctor_suffix}, {face[1]}{face_ctor_suffix}, {face[2]}{face_ctor_suffix} }},\n")
        f.write("        };\n\n")

        if meshlet_mode and chunks_count > 0:
            f.write(f"        alignas(16) static constexpr MeshChunk s_{var_name}Chunks[{chunks_count}] = {{\n")
            for ch in chunk_headers:
                f.write(
                    f"            {{ {ch['minX']}, {ch['minY']}, {ch['minZ']}, "
                    f"{ch['maxX']}, {ch['maxY']}, {ch['maxZ']}, "
                    f"{ch['vOffset']}u, {ch['faceOffset']}u, "
                    f"{ch['vCount']}u, {ch['faceCount']}u, "
                    f"{ch['normX']}, {ch['normY']}, {ch['normZ']}, {ch['coneDot']} }},\n"
                )
            f.write("        };\n")

        f.write("    }\n\n")

        f.write(f"    class {class_name} : public Mesh\n    {{\n    public:\n")
        f.write(f"        explicit {class_name}(float size = 1.0f)\n")
        if meshlet_mode and chunks_count > 0:
            f.write(f"            : Mesh(detail::s_{var_name}Vertices, {verts_count},\n")
            f.write(f"                   detail::s_{var_name}Faces, {faces_count},\n")
            f.write(f"                   true,\n")
            f.write(f"                   detail::s_{var_name}Chunks, {chunks_count})\n")
        else:
            f.write(f"            : Mesh(detail::s_{var_name}Vertices, {verts_count},\n")
            f.write(f"                   detail::s_{var_name}Faces, {faces_count},\n")
            f.write(f"                   true)\n")
        f.write("        {\n")
        f.write(f"            autoScale(size);\n")
        f.write(f"            finalizeGeometry({verts_count}, {faces_count},\n")
        f.write(f"                              Vector3({cx_ratio:.6f}f, {cy_ratio:.6f}f, {cz_ratio:.6f}f),\n")
        f.write(f"                              {radius_ratio:.6f}f);\n")
        f.write(f"            bindDeleter<{class_name}>();\n")
        f.write("        }\n    };\n}\n")

    return total_bytes, verts_count, faces_count, chunks_count


def convert_obj2mesh(obj_path, force_output_path=None, center_y=False, chunk_size=TILE_DEFAULT_CHUNK_SIZE):
    if not os.path.exists(obj_path):
        print(_err(f"Source file '{obj_path}' not found!"))
        sys.exit(1)

    src_basename = os.path.basename(obj_path)
    file_stem = os.path.splitext(src_basename)[0]

    meshlet_mode = file_stem.lower().endswith(CHUNK_SUFFIX)
    clean_stem = file_stem[:-len(CHUNK_SUFFIX)] if meshlet_mode else file_stem

    raw_vertices, raw_normals, raw_texcoords, parsed_triangles, has_uv = parse_obj(obj_path)

    if not parsed_triangles:
        print(_err(f"No valid faces in '{obj_path}'!"))
        sys.exit(1)

    if meshlet_mode:
        ordered_vertices, ordered_faces, chunk_headers, scale, centered_raw_verts = \
            build_clustered_mesh(parsed_triangles, raw_vertices, raw_normals, raw_texcoords,
                                 center_y, chunk_size)
    else:
        ordered_vertices, ordered_faces = build_flat_mesh(
            parsed_triangles, raw_vertices, raw_normals, raw_texcoords, center_y)
        chunk_headers = []

    px_all = [v['px'] for v in ordered_vertices]
    py_all = [v['py'] for v in ordered_vertices]
    pz_all = [v['pz'] for v in ordered_vertices]

    bcx, bcy, bcz, br = min_enclosing_ball(px_all, py_all, pz_all, iterations=128)

    verts_count = len(ordered_vertices)
    if chunk_headers:
        max_chunk_vcount = max(ch['vCount'] for ch in chunk_headers)
    else:
        max_chunk_vcount = verts_count
    use_face32 = max_chunk_vcount > 65535

    sanitized_name = clean_stem.replace('-', '_').replace(' ', '_')
    if not sanitized_name:
        sanitized_name = "Mesh"

    class_name = sanitized_name[0].upper() + sanitized_name[1:]
    var_name = sanitized_name.lower()

    if force_output_path:
        header_path = force_output_path
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        target_dir = None
        curr = script_dir
        for _ in range(4):
            test_path = os.path.join(curr, "src", "Models")
            if os.path.exists(test_path):
                target_dir = test_path
                break
            curr = os.path.dirname(curr)

        if target_dir:
            header_path = os.path.join(target_dir, sanitized_name + ".hpp")
        else:
            header_path = os.path.splitext(obj_path)[0] + ".hpp"

    total_bytes, verts_count, faces_count, chunks_count = write_header(
        header_path, class_name, var_name,
        ordered_vertices, ordered_faces, chunk_headers,
        bcx, bcy, bcz, br, use_face32,
        meshlet_mode, src_basename)

    rel_obj = src_basename
    rel_hpp = os.path.basename(header_path)
    mode_str = f"meshlet (chunks={chunks_count}, 32-bit={use_face32})" if meshlet_mode else f"flat (32-bit={use_face32})"
    print(_tag(f"Model: {rel_obj} -> {rel_hpp} ({verts_count} verts, {faces_count} tris, class={class_name}, mode={mode_str}, {total_bytes} bytes)"))
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pip3D OBJ converter (suffix '_chunk' in filename enables spatial chunking)")
    parser.prog = "Convert"
    parser.add_argument("input", help="Path to source .obj file")
    parser.add_argument("output", nargs="?", help="Optional output .hpp file path")
    parser.add_argument("--center-y", action="store_true", help="Center model vertically along Y axis")
    parser.add_argument("--chunk-size", type=int, default=TILE_DEFAULT_CHUNK_SIZE,
                        help=f"Max triangles per chunk (default: {TILE_DEFAULT_CHUNK_SIZE})")
    args = parser.parse_args()
    convert_obj2mesh(args.input, args.output, center_y=args.center_y, chunk_size=args.chunk_size)