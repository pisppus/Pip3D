import sys
sys.dont_write_bytecode = True

import math
import os
import argparse
import struct

from Obj import (_tag, _err, pack_normal, rgb888_to_565, rgb565_to_rgb888,
                    sanitize_asset_name, texture_hint, min_enclosing_ball,
                    cluster_triangles_kd, split_triangles_once,
                    compute_chunk_cone, parse_obj,
                    DEFAULT_CHUNK_SIZE, MAX_CHUNK_SIZE, DEFAULT_COLOR_565)

AUTO_CHUNK_MAX_FACES = 4096
POS_SUB_GRID = 128
from Instances import detect_instances
from Placements import convert_placements


def _float_lit(value, spec=".9g"):
    text = f"{float(value):{spec}}"
    if "." not in text and "e" not in text and "E" not in text:
        text += ".0"
    return text


def unpack_normal(oct16):
    nx = ((oct16 >> 8) & 0xFF) * (1.0 / 127.5) - 1.0
    ny = (oct16 & 0xFF) * (1.0 / 127.5) - 1.0
    nz = 1.0 - abs(nx) - abs(ny)
    if nz < 0.0:
        sx = 1.0 if nx >= 0.0 else -1.0
        sy = 1.0 if ny >= 0.0 else -1.0
        old_x = nx
        nx = (1.0 - abs(ny)) * sx
        ny = (1.0 - abs(old_x)) * sy
    length = math.sqrt(nx * nx + ny * ny + nz * nz)
    if length < 1e-9:
        return (0.0, 0.0, 1.0)
    return (nx / length, ny / length, nz / length)


def _pos_quant_setup(raw_vertices, ordered_triangles, center_y, center,
                     scale_override, pos_snap):
    min_x = min(v[0] for v in raw_vertices); max_x = max(v[0] for v in raw_vertices)
    min_y = min(v[1] for v in raw_vertices); max_y = max(v[1] for v in raw_vertices)
    min_z = min(v[2] for v in raw_vertices); max_z = max(v[2] for v in raw_vertices)

    cx = (min_x + max_x) * 0.5
    cz = (min_z + max_z) * 0.5
    cy = (min_y + max_y) * 0.5 if center_y else 0.0
    if center is not None:
        cx, cy, cz = center

    centered_raw_verts = [
        [v[0] - cx, v[1] - cy, v[2] - cz]
        for v in raw_vertices
    ]

    max_val = max(max(abs(v[0]), abs(v[1]), abs(v[2])) for v in centered_raw_verts)
    scale = 32767.0 / max_val if max_val > 1e-6 else 1.0
    if scale_override is not None:
        scale = scale_override

    half = pos_snap // 2
    grid_lim = 32768 - pos_snap

    mesh_ext = 0.0
    for tri in ordered_triangles:
        for corner in tri['corners']:
            v = centered_raw_verts[corner[0]]
            mesh_ext = max(mesh_ext, abs(v[0]), abs(v[1]), abs(v[2]))
    mesh_ext *= 2.0 * scale
    pos_snap = min(pos_snap, 1 << round(math.log2(max(1.0, mesh_ext / 1024.0))))
    half = pos_snap // 2
    grid_lim = 32768 - pos_snap

    def quant_pos(v):
        px = max(-grid_lim, min(grid_lim, int(round(v[0] * scale))))
        py = max(-grid_lim, min(grid_lim, int(round(v[1] * scale))))
        pz = max(-grid_lim, min(grid_lim, int(round(v[2] * scale))))
        return ((px + half) & ~(pos_snap - 1),
                (py + half) & ~(pos_snap - 1),
                (pz + half) & ~(pos_snap - 1))

    return centered_raw_verts, scale, pos_snap, quant_pos


def normals_redundant(parsed_triangles, raw_vertices, raw_normals, center_y):
    centered, _, _, quant_pos = _pos_quant_setup(raw_vertices, parsed_triangles,
                                                 center_y, None, None, POS_SUB_GRID)
    cos_tol = math.cos(math.radians(3.0))
    for tri in parsed_triangles:
        corners = tri['corners']
        qs = [quant_pos(centered[corner[0]]) for corner in corners]
        ax = qs[1][0] - qs[0][0]; ay = qs[1][1] - qs[0][1]; az = qs[1][2] - qs[0][2]
        bx = qs[2][0] - qs[0][0]; by = qs[2][1] - qs[0][1]; bz = qs[2][2] - qs[0][2]
        nx = ay * bz - az * by
        ny = az * bx - ax * bz
        nz = ax * by - ay * bx
        nlen = math.sqrt(nx * nx + ny * ny + nz * nz)
        if nlen < 1e-6:
            continue
        nx /= nlen; ny /= nlen; nz /= nlen
        for corner in corners:
            if corner[2] != -1 and corner[2] < len(raw_normals):
                n = raw_normals[corner[2]]
            else:
                n = tri['fallback_norm']
            if not n:
                return False
            on = unpack_normal(pack_normal(*n))
            dot = on[0] * nx + on[1] * ny + on[2] * nz
            if dot < cos_tol:
                return False
    return True

def _verify_chunk_stream(chunks, faces, data, expected, has_uv, store_normals):
    compact = not has_uv and not store_normals

    def fail(msg):
        print(_err(f"Verify: {msg}"))
        sys.exit(1)

    face_cursor = 0
    for ci, ch in enumerate(chunks):
        elist = expected[ci]
        epos, eattr, efaces = elist

        if ch['posCount'] > 255 or ch['attrCount'] > 255 or ch['faceCount'] > 255:
            fail(f"chunk {ci}: counts exceed uint8 "
                 f"(pos={ch['posCount']} attr={ch['attrCount']} face={ch['faceCount']})")
        if (ch['posCount'] != len(epos) or ch['attrCount'] != len(eattr)
                or ch['faceCount'] != len(efaces)):
            fail(f"chunk {ci}: header counts disagree with encoded content")
        if ch['faceOffset'] != face_cursor:
            fail(f"chunk {ci}: faceOffset {ch['faceOffset']} != running {face_cursor}")

        pos_rec = 3 if (ch['flags'] & 1) else 6
        off = ch['dataOffset']
        for pi in range(ch['posCount']):
            if pos_rec == 3:
                rx, ry, rz = data[off], data[off + 1], data[off + 2]
            else:
                rx, ry, rz = struct.unpack_from('<HHH', data, off)
            off += pos_rec
            sh = ch['posShift'] if pos_rec == 3 else 0
            got = (ch['minX'] + (rx << sh), ch['minY'] + (ry << sh),
                   ch['minZ'] + (rz << sh))
            if got != epos[pi]:
                fail(f"chunk {ci} pos {pi}: decoded {got} != expected {epos[pi]}")

        attr_off = ch['dataOffset'] + ch['posCount'] * pos_rec + ((ch['posCount'] * pos_rec) & 1)
        aoff = attr_off
        for ai in range(ch['attrCount']):
            if compact:
                got = (0, 0, 0)
            else:
                if has_uv:
                    qu, qv = struct.unpack_from('<HH', data, aoff)
                    aoff += 4
                else:
                    qu = qv = 0
                if store_normals:
                    octv, = struct.unpack_from('<H', data, aoff)
                    aoff += 2
                else:
                    octv = 0
                got = (qu, qv, octv)
            if got != eattr[ai]:
                fail(f"chunk {ci} attr {ai}: decoded {got} != expected {eattr[ai]}")

        for fi, rec in enumerate(efaces):
            if compact:
                p0, p1, p2 = rec
                a0 = a1 = a2 = 0
            else:
                p0, p1, p2, a0, a1, a2 = rec
            if max(p0, p1, p2) >= ch['posCount']:
                fail(f"chunk {ci} face {face_cursor + fi}: position index out of range {rec}")
            if not compact and max(a0, a1, a2) >= ch['attrCount']:
                fail(f"chunk {ci} face {face_cursor + fi}: attr index out of range {rec}")
            if faces[face_cursor + fi] != rec:
                fail(f"chunk {ci} face {face_cursor + fi}: face table mismatch")

        face_cursor += ch['faceCount']

    if face_cursor != len(faces):
        fail(f"face count mismatch: chunks cover {face_cursor}, table has {len(faces)}")


def build_chunked_mesh(ordered_triangles, raw_vertices, raw_normals, raw_texcoords,
                       center_y, chunk_size, store_normals, center=None,
                       scale_override=None, pos_snap=POS_SUB_GRID, store_uv=True):

    if chunk_size <= 0:
        chunk_size = (255 if len(ordered_triangles) <= AUTO_CHUNK_MAX_FACES
                      else DEFAULT_CHUNK_SIZE)
    chunk_size = max(1, min(int(chunk_size), MAX_CHUNK_SIZE))

    centered_raw_verts, scale, pos_snap, quant_pos = _pos_quant_setup(
        raw_vertices, ordered_triangles, center_y, center, scale_override, pos_snap)
    shift_cap = pos_snap.bit_length() - 1

    clustered_chunks = cluster_triangles_kd(ordered_triangles, max_chunk_size=chunk_size)

    has_uv = store_uv and len(raw_texcoords) > 0
    compact_faces = not has_uv and not store_normals

    uv_min_u, uv_max_u = 1e30, -1e30
    uv_min_v, uv_max_v = 1e30, -1e30
    if has_uv:
        for tri in ordered_triangles:
            for corner in tri['corners']:
                if corner[1] != -1 and corner[1] < len(raw_texcoords):
                    u, v = raw_texcoords[corner[1]]
                    v = 1.0 - v
                    uv_min_u = min(uv_min_u, u)
                    uv_max_u = max(uv_max_u, u)
                    uv_min_v = min(uv_min_v, v)
                    uv_max_v = max(uv_max_v, v)
        if uv_min_u > uv_max_u:
            uv_min_u, uv_max_u = 0.0, 1.0
        if uv_min_v > uv_max_v:
            uv_min_v, uv_max_v = 0.0, 1.0
        uv_span_u = (uv_max_u - uv_min_u) if (uv_max_u - uv_min_u) > 1e-9 else 1.0
        uv_span_v = (uv_max_v - uv_min_v) if (uv_max_v - uv_min_v) > 1e-9 else 1.0
    else:
        uv_min_u = uv_min_v = 0.0
        uv_span_u = uv_span_v = 1.0

    def quant_uv(u, v):
        if not has_uv:
            return 0, 0
        qu = max(0, min(65535, int(round((u - uv_min_u) * 65535.0 / uv_span_u))))
        qv = max(0, min(65535, int(round((v - uv_min_v) * 65535.0 / uv_span_v))))
        return qu, qv

    chunk_headers = []
    all_faces = []
    all_face_mats = []
    expected_chunks = []
    data = bytearray()
    total_pos = 0
    total_attr = 0
    delta8_chunks = 0
    face_offset = 0

    queue = list(clustered_chunks)
    while queue:
        ch = queue.pop(0)
        chunk_pos = {}
        chunk_attr = {}
        pos_list = []
        attr_list = []
        face_records = []

        for tri in ch:
            tri_indices_p = []
            tri_indices_a = []
            for corner in tri['corners']:
                pos = quant_pos(centered_raw_verts[corner[0]])
                if corner[0] not in chunk_pos:
                    chunk_pos[corner[0]] = len(pos_list)
                    pos_list.append(pos)
                p_idx = chunk_pos[corner[0]]

                if corner[2] != -1 and corner[2] < len(raw_normals):
                    norm = raw_normals[corner[2]]
                else:
                    norm = tri['fallback_norm']
                oct16 = pack_normal(*norm) if store_normals else 0

                if corner[1] != -1 and corner[1] < len(raw_texcoords):
                    tu, tv = raw_texcoords[corner[1]]
                else:
                    tu, tv = 0.0, 0.0
                tv = 1.0 - tv
                qu, qv = quant_uv(tu, tv)

                attr_key = (qu, qv, oct16)
                if attr_key not in chunk_attr:
                    chunk_attr[attr_key] = len(attr_list)
                    attr_list.append(attr_key)
                a_idx = chunk_attr[attr_key]

                tri_indices_p.append(p_idx)
                tri_indices_a.append(a_idx)

            if compact_faces:
                face_records.append(tuple(tri_indices_p))
            else:
                face_records.append((*tri_indices_p, *tri_indices_a))

        if len(pos_list) > 255 or len(attr_list) > 255 or len(face_records) > 255:
            left, right = split_triangles_once(ch)
            queue[:0] = [left, right]
            continue

        all_face_mats.extend(tri['material'] for tri in ch)

        ch_min_x = min(p[0] for p in pos_list); ch_max_x = max(p[0] for p in pos_list)
        ch_min_y = min(p[1] for p in pos_list); ch_max_y = max(p[1] for p in pos_list)
        ch_min_z = min(p[2] for p in pos_list); ch_max_z = max(p[2] for p in pos_list)

        ext = max(ch_max_x - ch_min_x, ch_max_y - ch_min_y, ch_max_z - ch_min_z)
        pos_shift = 0
        while pos_shift < shift_cap and ext > 255 << pos_shift:
            pos_shift += 1
        delta8 = ext <= 255 << pos_shift
        if delta8:
            delta8_chunks += 1

        chunk_data_offset = len(data)

        for (px, py, pz) in pos_list:
            if delta8:
                data += bytes(((px - ch_min_x) >> pos_shift,
                               (py - ch_min_y) >> pos_shift,
                               (pz - ch_min_z) >> pos_shift))
            else:
                data += struct.pack('<HHH', px - ch_min_x, py - ch_min_y, pz - ch_min_z)
        if len(data) & 1:
            data += b'\x00'

        for (qu, qv, oct16) in attr_list:
            if has_uv:
                data += struct.pack('<HH', qu, qv)
            if store_normals:
                data += struct.pack('<H', oct16)
        while len(data) & 3:
            data += b'\x00'

        oct_axis, cone_sin = compute_chunk_cone(ch, centered_raw_verts)

        chunk_headers.append({
            'minX': ch_min_x, 'minY': ch_min_y, 'minZ': ch_min_z,
            'maxX': ch_max_x, 'maxY': ch_max_y, 'maxZ': ch_max_z,
            'dataOffset': chunk_data_offset,
            'faceOffset': face_offset,
            'posCount': len(pos_list),
            'attrCount': len(attr_list),
            'faceCount': len(face_records),
            'flags': 1 if delta8 else 0,
            'posShift': pos_shift if delta8 else 0,
            'coneNormal': oct_axis,
            'coneSin': cone_sin,
        })
        all_faces.extend(face_records)
        expected_chunks.append((pos_list, attr_list, face_records))
        face_offset += len(face_records)
        total_pos += len(pos_list)
        total_attr += len(attr_list)

    _verify_chunk_stream(chunk_headers, all_faces, data, expected_chunks,
                         has_uv, store_normals)

    return {
        'chunks': chunk_headers,
        'faces': all_faces,
        'face_materials': all_face_mats,
        'data': bytes(data),
        'total_pos': total_pos,
        'total_attr': total_attr,
        'delta8_chunks': delta8_chunks,
        'scale': scale,
        'uv_min_u': uv_min_u, 'uv_span_u': uv_span_u,
        'uv_min_v': uv_min_v, 'uv_span_v': uv_span_v,
        'has_uv': has_uv,
        'store_normals': store_normals,
        'face_stride': 3 if compact_faces else 6,
    }


def build_submesh_ranges(face_materials, materials, default_color_565):

    has_colors = any(m is not None and m in materials for m in face_materials)
    if not has_colors:
        return []

    sub_meshes = []
    offset = 0
    i = 0
    n = len(face_materials)
    while i < n:
        m = face_materials[i]
        j = i
        while j < n and face_materials[j] == m:
            j += 1
        if m is not None and m in materials:
            r, g, b = materials[m]['color']
            color565 = rgb888_to_565(r, g, b)
            name = m
        else:
            color565 = default_color_565
            name = 'default' if m is None else m
        sub_meshes.append({
            'name': name,
            'color565': color565,
            'faceOffset': offset,
            'faceCount': j - i,
        })
        offset += j - i
        i = j
    return sub_meshes


def write_header(out_path, class_name, var_name, mesh, sub_meshes,
                 bcx, bcy, bcz, br, src_basename, texture_name=None):
    chunks = mesh['chunks']
    faces = mesh['faces']
    data = mesh['data']
    verts_count = mesh['total_pos']
    attr_count = mesh['total_attr']
    faces_count = len(faces)
    chunks_count = len(chunks)
    sub_count = len(sub_meshes)
    has_uv = mesh['has_uv']
    has_normals = mesh['store_normals']

    chunk_bytes = chunks_count * 28
    face_stride = mesh['face_stride']
    face_bytes = faces_count * face_stride
    submesh_bytes = sub_count * 12
    total_bytes = chunk_bytes + face_bytes + submesh_bytes + len(data)

    radius_ratio = br / 32767.0
    cx_ratio = bcx / 32767.0
    cy_ratio = bcy / 32767.0
    cz_ratio = bcz / 32767.0

    tex_hint = texture_hint(texture_name) if texture_name else None

    os.makedirs(os.path.dirname(out_path) or '.', exist_ok=True)

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("/*\n")
        f.write(f" * Pip3D Asset — {class_name}\n")
        f.write(" * Generated automatically by Tools/Models/Convert.py. Do not edit.\n")
        f.write(" *\n")
        f.write(f" * Source File     : {src_basename}\n")
        f.write(f" * Spatial Chunks  : {chunks_count} ({chunk_bytes} bytes, {mesh['delta8_chunks']} delta8)\n")
        if sub_count > 0:
            f.write(f" * Sub-Meshes      : {sub_count} ({submesh_bytes} bytes, material colors)\n")
            for sm in sub_meshes:
                r, g, b = rgb565_to_rgb888(sm['color565'])
                f.write(f" *   - {sm['name']:<20} faces {sm['faceOffset']}..{sm['faceOffset'] + sm['faceCount'] - 1:<6}"
                        f" 0x{sm['color565']:04X}  rgb({r},{g},{b})\n")
        if tex_hint:
            f.write(f" * Texture         : {texture_name}\n")
            if tex_hint['was_sanitized']:
                f.write(f" * Note            : texture name sanitized for C++ identifiers\n")
            if tex_hint['class']:
                if tex_hint['class'] == class_name:
                    f.write(f" * WARNING         : texture class '{tex_hint['class']}' clashes with mesh class — rename the source image\n")
                else:
                    f.write(f" * Include         : #include \"Textures/{tex_hint['hpp']}.hpp\"\n")
                    f.write(f" * Bind            : mesh->setTexture(&{tex_hint['var']});\n")
        streams = []
        streams.append("uv u16x2" if has_uv else None)
        streams.append("oct normal u16" if has_normals else None)
        streams = [s for s in streams if s]
        if streams:
            attr_note = ' + '.join(streams) + ", per-mesh optional"
        else:
            attr_note = "none (flat-shaded, normals derived at runtime)"
        f.write(f" * Attribute Slots : {attr_count} ({attr_note})\n")
        f.write(f" * Position Slots  : {verts_count} (chunk-local, u8/u16 deltas)\n")
        f.write(f" * Triangles       : {faces_count} ({face_bytes} bytes, {face_stride}B/tri, u8 chunk-local indices)\n")
        f.write(f" * Bounding Sphere : Center({cx_ratio:.4f}, {cy_ratio:.4f}, {cz_ratio:.4f}), Radius({radius_ratio:.4f})\n")
        f.write(f" * Flash Memory    : {total_bytes} bytes ({total_bytes / 1024.0:.2f} KB)\n")
        f.write(" */\n\n")

        f.write("#pragma once\n\n")
        f.write('#include "Geometry/Mesh.hpp"\n\n')
        f.write("namespace pip3D\n{\n")
        f.write("    namespace detail\n    {\n")

        _emit_geometry(f, var_name, mesh, sub_meshes)

        f.write("    }\n\n")

        f.write(f"    class {class_name} : public Mesh\n    {{\n    public:\n")
        f.write(f"        explicit {class_name}(float size = 1.0f)\n")
        _ctor_mesh_args(f, var_name, chunks_count, faces_count, has_uv,
                        has_normals, sub_meshes)
        f.write("        {\n")
        f.write("            autoScale(size);\n")
        f.write(f"            finalizeGeometry({verts_count}, {faces_count},\n")
        f.write(f"                              Vector3({cx_ratio:.6f}f, {cy_ratio:.6f}f, {cz_ratio:.6f}f) * (size * 0.5f),\n")
        f.write(f"                              {radius_ratio:.6f}f * (size * 0.5f));\n")
        if has_uv:
            f.write(f"            finalizeUVRange({_float_lit(mesh['uv_min_u'])}f, {_float_lit(mesh['uv_span_u'])}f,\n")
            f.write(f"                            {_float_lit(mesh['uv_min_v'])}f, {_float_lit(mesh['uv_span_v'])}f);\n")
        if texture_name:
            f.write(f"            setWantsTexture(true);\n")
        f.write(f"            bindDeleter<{class_name}>();\n")
        f.write("        }\n    };\n}\n")

    return total_bytes, verts_count, faces_count, chunks_count, sub_count


def _grid_positions(mesh):
    px_all, py_all, pz_all = [], [], []
    data = mesh['data']
    for ch in mesh['chunks']:
        off = ch['dataOffset']
        for _ in range(ch['posCount']):
            if ch['flags'] & 1:
                rx, ry, rz = data[off], data[off + 1], data[off + 2]
                off += 3
            else:
                rx, ry, rz = struct.unpack_from('<HHH', data, off)
                off += 6
            sh = ch['posShift'] if ch['flags'] & 1 else 0
            px_all.append(ch['minX'] + (rx << sh))
            py_all.append(ch['minY'] + (ry << sh))
            pz_all.append(ch['minZ'] + (rz << sh))
    return px_all, py_all, pz_all


def _emit_geometry(f, var_name, mesh, sub_meshes):
    chunks = mesh['chunks']
    data = mesh['data']

    f.write(f"        alignas(4) static constexpr MeshChunk s_{var_name}Chunks[{len(chunks)}] = {{\n")
    for ch in chunks:
        f.write(
            f"            {{ {ch['minX']}, {ch['minY']}, {ch['minZ']}, "
            f"{ch['maxX']}, {ch['maxY']}, {ch['maxZ']}, "
            f"{ch['dataOffset']}u, {ch['faceOffset']}u, "
            f"{ch['posCount']}, {ch['attrCount']}, {ch['faceCount']}, {ch['flags']}, "
            f"{ch['coneNormal']}, {ch['coneSin']}, {ch['posShift']} }},\n")
    f.write("        };\n\n")

    if sub_meshes:
        f.write(f"        alignas(4) static constexpr SubMesh s_{var_name}SubMeshes[{len(sub_meshes)}] = {{\n")
        for sm in sub_meshes:
            f.write(
                f"            {{ {sm['faceOffset']}u, {sm['faceCount']}u, "
                f"Color(0x{sm['color565']:04X}u) /* {sm['name']} */ }},\n")
        f.write("        };\n\n")

    blob = bytearray()
    for rec in mesh['faces']:
        blob += bytes(rec)
    stream_offset = len(blob)
    blob += data

    f.write(f"        static constexpr uint32_t s_{var_name}StreamOffset = {stream_offset}u;\n\n")
    f.write(f"        alignas(4) static constexpr uint8_t s_{var_name}Geometry[{len(blob)}] = {{\n")
    for i in range(0, len(blob), 16):
        f.write("            " + "".join(f"0x{b:02X}," for b in blob[i:i + 16]) + "\n")
    f.write("        };\n\n")


def _ctor_mesh_args(f, var_name, chunks_count, faces_count, has_uv, has_normals,
                    sub_meshes):
    f.write(f"            : Mesh(detail::s_{var_name}Chunks, {chunks_count},\n")
    f.write(f"                   detail::s_{var_name}Geometry + detail::s_{var_name}StreamOffset,\n")
    f.write(f"                   reinterpret_cast<const Face *>(detail::s_{var_name}Geometry), {faces_count},\n")
    f.write(f"                   {'true' if has_uv else 'false'}, "
            f"{'true' if has_normals else 'false'}, true,\n")
    if sub_meshes:
        f.write(f"                   detail::s_{var_name}SubMeshes, {len(sub_meshes)})\n")
    else:
        f.write("                   nullptr, 0)\n")


def _emit_mesh_class(f, class_name, var_name, mesh, sub_meshes, natural, bounds,
                     has_normals, wants_texture=False):
    bcx, bcy, bcz, br = bounds
    verts_count = mesh['total_pos']
    faces_count = len(mesh['faces'])
    chunks_count = len(mesh['chunks'])
    sub_count = len(sub_meshes)
    f.write(f"    class {class_name} : public Mesh\n    {{\n    public:\n")
    natural_lit = _float_lit(natural, ".6g")
    f.write(f"        explicit {class_name}(float size = {natural_lit}f)\n")
    _ctor_mesh_args(f, var_name, chunks_count, faces_count, mesh['has_uv'],
                    has_normals, sub_meshes)
    f.write("        {\n")
    f.write("            autoScale(size);\n")
    f.write(f"            finalizeGeometry({verts_count}, {faces_count},\n")
    f.write(f"                              Vector3({bcx / 32767.0:.6f}f, {bcy / 32767.0:.6f}f, "
            f"{bcz / 32767.0:.6f}f) * (size * 0.5f),\n")
    f.write(f"                              {br / 32767.0:.6f}f * (size * 0.5f));\n")
    if mesh['has_uv']:
        f.write(f"            finalizeUVRange({_float_lit(mesh['uv_min_u'])}f, {_float_lit(mesh['uv_span_u'])}f,\n")
        f.write(f"                            {_float_lit(mesh['uv_min_v'])}f, {_float_lit(mesh['uv_span_v'])}f);\n")
    if wants_texture:
        f.write(f"            setWantsTexture(true);\n")
    f.write(f"            bindDeleter<{class_name}>();\n")
    f.write("        }\n    };\n\n")


def _convert_tile(header_path, class_name, var_name, src_basename, primary_texture,
                  parsed_triangles, raw_vertices, raw_normals, raw_texcoords, materials,
                  props, consumed, center_y, chunk_size, store_normals,
                  default_color_565, pos_snap, store_uv=True):
    min_x = min(v[0] for v in raw_vertices); max_x = max(v[0] for v in raw_vertices)
    min_y = min(v[1] for v in raw_vertices); max_y = max(v[1] for v in raw_vertices)
    min_z = min(v[2] for v in raw_vertices); max_z = max(v[2] for v in raw_vertices)
    cx = (min_x + max_x) * 0.5
    cz = (min_z + max_z) * 0.5
    cy = (min_y + max_y) * 0.5 if center_y else 0.0

    max_val = max(max(abs(v[0] - cx), abs(v[1] - cy), abs(v[2] - cz))
                  for v in raw_vertices)
    grid = 32767.0 / max_val if max_val > 1e-6 else 1.0
    natural = 2.0 * max_val

    remainder_tris = [t for i, t in enumerate(parsed_triangles) if i not in consumed]
    remainder = None
    if remainder_tris:
        remainder = build_chunked_mesh(remainder_tris, raw_vertices, raw_normals,
                                       raw_texcoords, center_y, chunk_size,
                                       store_normals, center=(cx, cy, cz),
                                       scale_override=grid, pos_snap=pos_snap,
                                       store_uv=store_uv)

    prop_meshes = []
    for p in props:
        prop_tris = [parsed_triangles[ti] for ti in p['tris']]
        pm = build_chunked_mesh(prop_tris, raw_vertices, raw_normals, raw_texcoords,
                                center_y, chunk_size, store_normals,
                                center=p['center'], scale_override=grid,
                                pos_snap=pos_snap, store_uv=store_uv)
        prop_meshes.append(pm)

    os.makedirs(os.path.dirname(header_path) or '.', exist_ok=True)
    tex_hint = texture_hint(primary_texture) if primary_texture else None
    wants_texture = tex_hint is not None
    with open(header_path, "w", encoding="utf-8") as f:
        f.write("/*\n")
        f.write(f" * Pip3D Asset — {class_name}\n")
        f.write(" * Generated automatically by Tools/Models/Convert.py. Do not edit.\n")
        f.write(f" * Source File     : {src_basename}\n")
        total = sum(len(m['data']) + len(m['faces']) * m['face_stride'] + len(m['chunks']) * 28
                    for m in ([remainder] if remainder else []) + prop_meshes)
        f.write(f" * Props           : {len(props)}\n")
        f.write(f" * Placements      : {sum(1 + len(p['members']) for p in props)}\n")
        if tex_hint:
            f.write(f" * Texture         : {primary_texture}\n")
            if tex_hint['was_sanitized']:
                f.write(f" * Note            : texture name sanitized for C++ identifiers\n")
            if tex_hint['class']:
                if tex_hint['class'] == class_name:
                    f.write(f" * WARNING         : texture class '{tex_hint['class']}' clashes with mesh class — rename the source image\n")
                else:
                    f.write(f" * Include         : #include \"Textures/{tex_hint['hpp']}.hpp\"\n")
                    f.write(f" * Bind            : mesh->setTexture(&{tex_hint['var']});  // on {class_name} and every prop class\n")
        f.write(f" * Flash Memory    : {total} bytes ({total / 1024.0:.2f} KB)\n")
        f.write(" */\n\n")

        f.write("#pragma once\n\n")
        f.write('#include "Geometry/Instance.hpp"\n\n')
        f.write("namespace pip3D\n{\n    namespace detail\n    {\n")
        prop_subs = [build_submesh_ranges(pm['face_materials'], materials,
                                          default_color_565)
                     for pm in prop_meshes]
        rsub = (build_submesh_ranges(remainder['face_materials'], materials,
                                     default_color_565)
                if remainder else [])
        for p, pm, psub in zip(props, prop_meshes, prop_subs):
            _emit_geometry(f, p['name'].lower(), pm, psub)
        if remainder:
            _emit_geometry(f, var_name, remainder, rsub)
        f.write("    }\n\n")

        if remainder:
            bx, by, bz, br = min_enclosing_ball(*_grid_positions(remainder), iterations=128)
            _emit_mesh_class(f, class_name, var_name, remainder, rsub, natural,
                             (bx, by, bz, br), store_normals, wants_texture)

        for p, pm, psub in zip(props, prop_meshes, prop_subs):
            pb = min_enclosing_ball(*_grid_positions(pm), iterations=128)
            pclass = p['name'][0].upper() + p['name'][1:]
            _emit_mesh_class(f, pclass, p['name'].lower(), pm, psub, natural, pb,
                             store_normals, wants_texture)

        f.write("    namespace " + var_name + "_tile\n    {\n")
        for p in props:
            pclass = p["name"][0].upper() + p["name"][1:]
            f.write("    static " + pclass + " " + pclass[0].lower() + pclass[1:] + "Obj;\n")
        f.write("\n")

        f.write("    static const Placement placements["
                + str(sum(1 + len(p['members']) for p in props)) + "] = {\n")
        for pi, (p, pm) in enumerate(zip(props, prop_meshes)):
            px = max(-32767, min(32767, int(round((p['center'][0] - cx) * grid))))
            py = max(-32767, min(32767, int(round((p['center'][1] - cy) * grid))))
            pz = max(-32767, min(32767, int(round((p['center'][2] - cz) * grid))))
            f.write(f"        {{ {pi}, {px}, {py}, {pz}, 0, "
                    f"kPlacementVisible, Color(0xFFFFu) }},\n")
            for m, (yaw, cb) in p['members']:
                mx = max(-32767, min(32767, int(round((cb[0] - cx) * grid))))
                my = max(-32767, min(32767, int(round((cb[1] - cy) * grid))))
                mz = max(-32767, min(32767, int(round((cb[2] - cz) * grid))))
                f.write(f"        {{ {pi}, {mx}, {my}, {mz}, {yaw}, "
                        f"kPlacementVisible, Color(0xFFFFu) }},\n")
        f.write("    };\n\n")

        f.write("    static Mesh *const props[] = {\n")
        for p in props:
            pclass = p['name'][0].upper() + p['name'][1:]
            f.write(f"        &{pclass[0].lower() + pclass[1:]}Obj,\n")
        f.write("    };\n\n")
        f.write("    static const PlacementSet set = {\n")
        f.write(f"        placements, {sum(1 + len(p['members']) for p in props)},\n")
        f.write(f"        props, {len(props)},\n")
        f.write(f"        {_float_lit(1.0 / grid)}f, true\n")
        f.write("    };\n    }\n}\n")

    return natural, grid


def parse_color_arg(s):
    parts = s.split(',')
    if len(parts) != 3:
        print(_err(f"--default-color '{s}': expected r,g,b in 0..255"))
        sys.exit(1)
    try:
        r, g, b = int(parts[0]), int(parts[1]), int(parts[2])
    except ValueError:
        print(_err(f"--default-color '{s}': expected r,g,b in 0..255"))
        sys.exit(1)
    if any(v < 0 or v > 255 for v in (r, g, b)):
        print(_err(f"--default-color '{s}': values must be in 0..255"))
        sys.exit(1)
    return rgb888_to_565(r, g, b)


def convert_obj(obj_path, force_output_path=None, center_y=False,
                     chunk_size=DEFAULT_CHUNK_SIZE,
                     default_color_565=DEFAULT_COLOR_565,
                     store_normals=True, pos_snap=POS_SUB_GRID,
                     uv_mode="auto", normals_mode="auto"):
    if not os.path.exists(obj_path):
        print(_err(f"Source file '{obj_path}' not found!"))
        sys.exit(1)

    src_basename = os.path.basename(obj_path)
    file_stem = os.path.splitext(src_basename)[0]

    CHUNK_SUFFIX = "_chunk"
    meshlet_mode = file_stem.lower().endswith(CHUNK_SUFFIX)
    clean_stem = file_stem[:-len(CHUNK_SUFFIX)] if meshlet_mode else file_stem

    raw_vertices, raw_normals, raw_texcoords, parsed_triangles, has_uv, materials, primary_texture = parse_obj(obj_path)

    if not parsed_triangles:
        print(_err(f"No valid faces in '{obj_path}'!"))
        sys.exit(1)

    if uv_mode == "auto":
        store_uv = primary_texture is not None
        if raw_texcoords and not store_uv:
            print(_tag(f"  UVs dropped: no texture hint in materials "
                       f"(pass --keep-uv to store them anyway)"))
    elif uv_mode == "keep":
        store_uv = True
        if not raw_texcoords:
            print(_tag(f"  --keep-uv ignored: source has no UV coordinates"))
    else:
        store_uv = False

    if normals_mode == "drop":
        store_normals = False
    elif normals_mode == "keep":
        store_normals = True
    elif store_normals and normals_redundant(parsed_triangles, raw_vertices,
                                             raw_normals, center_y):
        store_normals = False
        print(_tag(f"  Normals dropped: identical to the geometric face "
                   f"normals (hard-edged geometry); saves 2 bytes per corner"))

    props, consumed = detect_instances(parsed_triangles, raw_vertices, materials, clean_stem)

    sanitized_name = sanitize_asset_name(clean_stem)
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
        header_path = (os.path.join(target_dir, sanitized_name + ".hpp")
                       if target_dir else os.path.splitext(obj_path)[0] + ".hpp")

    if props:
        natural, grid = _convert_tile(
            header_path, class_name, var_name, src_basename, primary_texture,
            parsed_triangles, raw_vertices, raw_normals, raw_texcoords, materials,
            props, consumed, center_y, chunk_size, store_normals, default_color_565,
            pos_snap, store_uv)
        print(_tag(f"Tile: {src_basename} -> {os.path.basename(header_path)} "
                   f"({len(props)} props, {sum(1 + len(p['members']) for p in props)} placements, "
                   f"natural size {natural:.3f}, unitScale {1.0 / grid:.9g})"))
        return True

    mesh = build_chunked_mesh(parsed_triangles, raw_vertices, raw_normals, raw_texcoords,
                              center_y, chunk_size, store_normals, pos_snap=pos_snap,
                              store_uv=store_uv)
    sub_meshes = build_submesh_ranges(mesh['face_materials'], materials, default_color_565)

    bcx, bcy, bcz, br = min_enclosing_ball(*_grid_positions(mesh), iterations=128)

    total_bytes, verts_count, faces_count, chunks_count, sub_count = write_header(
        header_path, class_name, var_name, mesh, sub_meshes,
        bcx, bcy, bcz, br, src_basename, primary_texture)

    rel_obj = src_basename
    rel_hpp = os.path.basename(header_path)
    print(_tag(f"Model: {rel_obj} -> {rel_hpp} ({verts_count} pos slots, {faces_count} tris, "
               f"chunks={chunks_count} ({mesh['delta8_chunks']} delta8), submeshes={sub_count}, "
               f"uv={'y' if mesh['has_uv'] else 'n'}, normals={'y' if store_normals else 'n'}, "
               f"{total_bytes} bytes)"))
    if primary_texture:
        hint = texture_hint(primary_texture)
        if hint['was_sanitized']:
            print(_err(f"  Texture name '{primary_texture}' contains invalid C++ chars; will be sanitized to '{hint['sanitized']}'."))
            print(_err(f"  Rename source file to '{hint['sanitized']}.png' for clarity."))
        if not hint['class']:
            print(_err(f"  Texture '{primary_texture}' yields an empty C++ asset name."))
        elif hint['class'] == class_name:
            print(_err(f"  Texture '{primary_texture}' → class '{hint['class']}' clashes with mesh class!"))
            print(_err(f"  Rename PNG to '{var_name}_tex.png' (or similar) to avoid C++ conflict."))
        else:
            print(_tag(f"  Texture: {primary_texture} -> #include \"Textures/{hint['hpp']}.hpp\", mesh->setTexture(&{hint['var']});"))
    if sub_count > 1:
        print(_tag(f"  Materials:"))
        for sm in sub_meshes:
            r8, g8, b8 = rgb565_to_rgb888(sm['color565'])
            print(_tag(f"    - {sm['name']:<24} RGB=({r8:3d},{g8:3d},{b8:3d})  faces {sm['faceOffset']}..{sm['faceOffset']+sm['faceCount']-1}"))
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pip3D OBJ converter — chunked split-stream format")
    parser.prog = "Convert"
    parser.add_argument("pairs", nargs="*",
                        help="input [output], or in1 out1 in2 out2 ... for batch")
    parser.add_argument("--center-y", action="store_true", help="Center model vertically along Y axis")
    parser.add_argument("--chunk-size", type=int, default=0,
                        help=f"Triangles per chunk, {MAX_CHUNK_SIZE} max, 0 = auto by mesh size")
    parser.add_argument("--default-color", type=str, default="128,128,128",
                        help="Fallback material color as r,g,b in 0..255 for materials without a Kd color (default: 128,128,128)")
    parser.add_argument("--pos-snap", type=int, default=POS_SUB_GRID,
                        help="Max position snap in grid steps, power of 2 (default: %d); "
                             "the converter scales it down per mesh to keep worst-case "
                             "vertex error near half a percent of the mesh size" % POS_SUB_GRID)
    parser.add_argument("--no-normals", action="store_true",
                        help="Do not store per-corner normals (face normals are derived at runtime; static/lightmapped geometry)")
    parser.add_argument("--keep-normals", action="store_true",
                        help="Always store per-corner normals, skipping the automatic redundancy check")
    parser.add_argument("--keep-uv", action="store_true",
                        help="Store UV coordinates even when no texture hint is present in the materials")
    parser.add_argument("--no-uv", action="store_true",
                        help="Never store UV coordinates, even when a texture hint is present")
    parser.add_argument("--placements", nargs=2, metavar=("IN", "OUT"),
                        help="Placement list txt -> flash-resident placement header")
    args = parser.parse_args()

    if args.placements:
        convert_placements(args.placements[0], args.placements[1])
        sys.exit(0)

    if args.keep_uv and args.no_uv:
        print(_err("--keep-uv and --no-uv are mutually exclusive"))
        sys.exit(1)
    if args.no_normals and args.keep_normals:
        print(_err("--no-normals and --keep-normals are mutually exclusive"))
        sys.exit(1)
    uv_mode = "keep" if args.keep_uv else ("drop" if args.no_uv else "auto")
    normals_mode = "drop" if args.no_normals else ("keep" if args.keep_normals else "auto")

    default_color_565 = parse_color_arg(args.default_color)

    files = args.pairs
    if len(files) % 2 != 0:
        if len(files) == 1:
            files = [files[0], None]
        else:
            parser.error("expected input/output pairs: in1 out1 [in2 out2 ...]")

    pos_snap = args.pos_snap
    if pos_snap < 1 or pos_snap > 128 or (pos_snap & (pos_snap - 1)) != 0:
        print(_err(f"--pos-snap {pos_snap}: power of 2 in 1..128"))
        sys.exit(1)

    for i in range(0, len(files), 2):
        convert_obj(files[i], files[i + 1],
                         center_y=args.center_y,
                         chunk_size=args.chunk_size,
                         default_color_565=default_color_565,
                         store_normals=not args.no_normals,
                         pos_snap=pos_snap,
                         uv_mode=uv_mode,
                         normals_mode=normals_mode)
