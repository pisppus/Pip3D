import sys
sys.dont_write_bytecode = True

import os
import math

if os.name == 'nt':
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)


def _tag(msg):
    return f"\033[36m[Pip3D]\033[0m {msg}"


def _err(msg):
    return f"\033[91m[-] Error: {msg}\033[0m"

DEFAULT_CHUNK_SIZE = 64
MAX_CHUNK_SIZE = 255
DEFAULT_COLOR_565 = 0x8410


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


def rgb888_to_565(r, g, b):
    r = max(0, min(255, int(r)))
    g = max(0, min(255, int(g)))
    b = max(0, min(255, int(b)))
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def rgb565_to_rgb888(c565):
    r5 = (c565 >> 11) & 0x1F
    g6 = (c565 >> 5) & 0x3F
    b5 = c565 & 0x1F
    return (r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2)


def sanitize_asset_name(raw_stem):
    sanitized = ''.join(c if (c.isascii() and c.isalnum() or c == '_') else '_' for c in raw_stem)
    if sanitized and sanitized[0].isdigit():
        sanitized = 'tex_' + sanitized
    return sanitized


def _is_pot_suffix(token):

    try:
        if 'x' in token:
            wp, hp = token.split('x')
            wv, hv = int(wp), int(hp)
        else:
            wv = hv = int(token)
        return wv >= 16 and (wv & (wv - 1)) == 0 and hv >= 16 and (hv & (hv - 1)) == 0
    except ValueError:
        return False


def texture_class_name(tex_stem):
    parts = tex_stem.split('_')
    name = tex_stem
    if len(parts) > 1 and _is_pot_suffix(parts[-1]):
        name = '_'.join(parts[:-1])
    return name[0].upper() + name[1:] if name else ''


def texture_hint(texture_name):

    stem = os.path.splitext(texture_name)[0]
    sanitized = sanitize_asset_name(stem)
    cls = texture_class_name(sanitized) or None
    return {
        'stem': stem,
        'sanitized': sanitized,
        'was_sanitized': sanitized != stem,
        'class': cls,
        'var': ('g_' + cls.lower() + 'Texture') if cls else None,
        'hpp': (cls[0].lower() + cls[1:]) if cls else '',
    }


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


def split_triangles_once(tri_list):
    min_x = min(t['cx'] for t in tri_list); max_x = max(t['cx'] for t in tri_list)
    min_y = min(t['cy'] for t in tri_list); max_y = max(t['cy'] for t in tri_list)
    min_z = min(t['cz'] for t in tri_list); max_z = max(t['cz'] for t in tri_list)

    dx = max_x - min_x
    dy = max_y - min_y
    dz = max_z - min_z

    if dx >= dy and dx >= dz:
        axis = 'cx'
    elif dy >= dz:
        axis = 'cy'
    else:
        axis = 'cz'

    tri_list.sort(key=lambda t: t[axis])
    mid = len(tri_list) // 2
    return tri_list[:mid], tri_list[mid:]


def cluster_triangles_kd(tri_list, max_chunk_size):

    chunks = []

    def bisect(subset):
        if len(subset) <= max_chunk_size:
            chunks.append(subset)
            return
        left, right = split_triangles_once(subset)
        bisect(left)
        bisect(right)

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

    if min_dot <= 0.0:
        return 0, 0

    oct16 = pack_normal(avg_nx, avg_ny, avg_nz)
    sin_half = math.sqrt(max(0.0, 1.0 - min_dot * min_dot))
    q8 = max(1, min(255, int(round(sin_half * 255.0))))
    return oct16, q8


def parse_mtl(mtl_path):
    materials = {}
    primary_texture = None
    if not mtl_path or not os.path.exists(mtl_path):
        return materials, primary_texture
    current_name = None
    try:
        with open(mtl_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                parts = line.strip().split()
                if not parts:
                    continue
                token = parts[0]
                if token == 'newmtl':
                    current_name = parts[1] if len(parts) > 1 else 'default'
                    if current_name not in materials:
                        materials[current_name] = {'color': (200, 200, 200), 'texture': None}
                elif token == 'Kd' and current_name is not None and len(parts) >= 4:
                    try:
                        r = float(parts[1]) * 255.0
                        g = float(parts[2]) * 255.0
                        b = float(parts[3]) * 255.0
                        materials[current_name]['color'] = (
                            max(0, min(255, int(round(r)))),
                            max(0, min(255, int(round(g)))),
                            max(0, min(255, int(round(b))))
                        )
                    except ValueError:
                        pass
                elif token == 'map_Kd' and current_name is not None and len(parts) >= 2:
                    tex_name = os.path.basename(parts[-1])
                    if not tex_name.lower().endswith(('.png', '.jpg', '.jpeg', '.tga', '.bmp')):
                        continue
                    materials[current_name]['texture'] = tex_name
                    if primary_texture is None:
                        primary_texture = tex_name
    except Exception as e:
        print(_err(f"Parsing MTL '{mtl_path}': {str(e)}"))
    return materials, primary_texture


def find_mtl_for_obj(obj_path):
    mtl_path = None
    try:
        with open(obj_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                parts = line.strip().split()
                if parts and parts[0] == 'mtllib':
                    mtl_name = ' '.join(parts[1:])
                    candidate = os.path.join(os.path.dirname(obj_path), mtl_name)
                    if os.path.exists(candidate):
                        mtl_path = candidate
                        break
    except Exception:
        pass
    if mtl_path is None:
        candidate = os.path.splitext(obj_path)[0] + '.mtl'
        if os.path.exists(candidate):
            mtl_path = candidate
    return mtl_path


def parse_obj(obj_path):
    raw_vertices = []
    raw_normals = []
    raw_texcoords = []
    parsed_triangles = []
    has_uv = False

    mtl_path = find_mtl_for_obj(obj_path)
    materials, primary_texture = parse_mtl(mtl_path) if mtl_path else ({}, None)
    current_material = None

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
                elif token == 'usemtl':
                    current_material = parts[1] if len(parts) > 1 else None
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
                            'cz': (p0[2] + p1[2] + p2[2]) / 3.0,
                            'material': current_material
                        })
    except Exception as e:
        print(_err(f"Parsing OBJ: {str(e)}"))
        sys.exit(1)

    return raw_vertices, raw_normals, raw_texcoords, parsed_triangles, has_uv, materials, primary_texture
