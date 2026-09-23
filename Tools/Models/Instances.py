import sys
sys.dont_write_bytecode = True

import math

from Obj import sanitize_asset_name


def _weld_key(v):
    return (round(v[0], 4), round(v[1], 4), round(v[2], 4))


def split_components(parsed_triangles, raw_vertices):
    parent = {}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for tri in parsed_triangles:
        keys = [_weld_key(raw_vertices[c[0]]) for c in tri['corners']]
        for k in keys:
            if k not in parent:
                parent[k] = k
        for k in keys[1:]:
            ra, rb = find(keys[0]), find(k)
            if ra != rb:
                parent[ra] = rb

    comps = []
    comp_of_root = {}
    for ti, tri in enumerate(parsed_triangles):
        root = find(_weld_key(raw_vertices[tri['corners'][0][0]]))
        ci = comp_of_root.get(root)
        if ci is None:
            ci = len(comps)
            comp_of_root[root] = ci
            comps.append({'tris': [], 'keys': set()})
        comps[ci]['tris'].append(ti)
        for c in tri['corners']:
            comps[ci]['keys'].add(_weld_key(raw_vertices[c[0]]))
    return comps


def _centroid(comp, parsed_triangles, raw_vertices):
    sx = sy = sz = 0.0
    n = 0
    for ti in comp['tris']:
        for c in parsed_triangles[ti]['corners']:
            v = raw_vertices[c[0]]
            sx += v[0]
            sy += v[1]
            sz += v[2]
            n += 1
    return (sx / n, sy / n, sz / n)


def _tri_edges(tri, raw_vertices):
    p = [raw_vertices[c[0]] for c in tri['corners']]
    lens = []
    for i in range(3):
        a, b = p[i], p[(i + 1) % 3]
        lens.append(round(math.sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2
                                    + (a[2] - b[2]) ** 2), 4))
    return tuple(sorted(lens))


def _fingerprint(comp, parsed_triangles, raw_vertices):
    items = sorted((_tri_edges(parsed_triangles[ti], raw_vertices),
                    parsed_triangles[ti]['material'])
                   for ti in comp['tris'])
    return tuple(items)


def _prop_bytes(comp, parsed_triangles, raw_vertices):
    corners = len(comp['keys'])
    tris = len(comp['tris'])
    return corners * 6 + corners * 6 + tris * 6 + 28


def _match_member(comp_a, comp_b, parsed_triangles, raw_vertices):
    if len(comp_a['keys']) != len(comp_b['keys']):
        return None
    if len(comp_a['tris']) != len(comp_b['tris']):
        return None

    ca = _centroid(comp_a, parsed_triangles, raw_vertices)
    cb = _centroid(comp_b, parsed_triangles, raw_vertices)

    ext = 0.0
    for ti in comp_a['tris']:
        for c in parsed_triangles[ti]['corners']:
            v = raw_vertices[c[0]]
            ext = max(ext, abs(v[0] - ca[0]), abs(v[1] - ca[1]), abs(v[2] - ca[2]))
    eps = max(2e-3, ext * 2e-3)

    ref = None
    for ti in comp_a['tris']:
        lens = _tri_edges(parsed_triangles[ti], raw_vertices)
        if len(set(lens)) == 3:
            ref = (ti, lens)
            break
    if ref is None and comp_a['tris']:
        ti = comp_a['tris'][0]
        ref = (ti, _tri_edges(parsed_triangles[ti], raw_vertices))
    if ref is None:
        return None

    a_tri = parsed_triangles[ref[0]]
    a_rel = [tuple(raw_vertices[c[0]][i] - ca[i] for i in range(3))
             for c in a_tri['corners']]

    a_set = set()
    for ti in comp_a['tris']:
        for c in parsed_triangles[ti]['corners']:
            v = raw_vertices[c[0]]
            a_set.add((round((v[0] - ca[0]) / eps), round((v[1] - ca[1]) / eps),
                       round((v[2] - ca[2]) / eps)))

    by_edges = {}
    for ti in comp_b['tris']:
        by_edges.setdefault(_tri_edges(parsed_triangles[ti], raw_vertices),
                            []).append(ti)

    def verify(rotated_b):
        for v in rotated_b:
            gx = (v[0] - cb[0]) / eps
            gy = (v[1] - cb[1]) / eps
            gz = (v[2] - cb[2]) / eps
            hit = False
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for dz in (-1, 0, 1):
                        if (round(gx) + dx, round(gy) + dy, round(gz) + dz) in a_set:
                            hit = True
                            break
                    if hit:
                        break
                if hit:
                    break
            if not hit:
                return False
        return True

    b_corners = []
    for ti in comp_b['tris']:
        for c in parsed_triangles[ti]['corners']:
            w = raw_vertices[c[0]]
            b_corners.append((w[0] - cb[0], w[1] - cb[1], w[2] - cb[2]))

    for ti_b in by_edges.get(ref[1], []):
        b_tri = parsed_triangles[ti_b]
        b_rel = [tuple(raw_vertices[c[0]][i] - cb[i] for i in range(3))
                 for c in b_tri['corners']]

        for perm in ((0, 1, 2), (0, 2, 1), (1, 0, 2), (1, 2, 0), (2, 0, 1), (2, 1, 0)):
            u = (a_rel[1][0] - a_rel[0][0], a_rel[1][2] - a_rel[0][2])
            v = (b_rel[perm[1]][0] - b_rel[perm[0]][0],
                 b_rel[perm[1]][2] - b_rel[perm[0]][2])
            if math.hypot(u[0], u[1]) < 1e-9 or math.hypot(v[0], v[1]) < 1e-9:
                continue
            dot = u[0] * v[0] + u[1] * v[1]
            cross = u[0] * v[1] - u[1] * v[0]
            theta = math.atan2(cross, dot)

            for sign in (1.0, -1.0):
                tq = round((sign * theta) / (2.0 * math.pi) * 256.0) % 256
                th = tq / 256.0 * 2.0 * math.pi
                cs, sn = math.cos(th), math.sin(th)
                rotated = [(cb[0] + x * cs + z * sn, cb[1] + y,
                            cb[2] + (-x * sn + z * cs))
                           for (x, y, z) in b_corners]
                if verify(rotated):
                    return (256 - tq) % 256, cb
    return None


def detect_instances(parsed_triangles, raw_vertices, materials, tile_stem):
    comps = split_components(parsed_triangles, raw_vertices)
    if len(comps) < 2:
        return [], set()

    stem = sanitize_asset_name(tile_stem) or 'Tile'

    groups = {}
    for ci, comp in enumerate(comps):
        groups.setdefault(_fingerprint(comp, parsed_triangles, raw_vertices),
                          []).append(ci)

    props = []
    consumed = set()
    pid = 0
    for fp, members in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        if len(members) < 2:
            continue
        b = _prop_bytes(comps[members[0]], parsed_triangles, raw_vertices)
        if (len(members) - 1) * b <= len(members) * 12 + 64:
            continue

        canonical = members[0]
        ca = _centroid(comps[canonical], parsed_triangles, raw_vertices)
        inst = []
        for m in members[1:]:
            r = _match_member(comps[canonical], comps[m], parsed_triangles,
                              raw_vertices)
            if r is not None:
                inst.append((m, r))
        if not inst:
            continue

        props.append({
            'name': stem + '_p' + str(pid),
            'tris': list(comps[canonical]['tris']),
            'center': ca,
            'members': inst,
        })
        pid += 1
        consumed.update(comps[canonical]['tris'])
        for m, _ in inst:
            consumed.update(comps[m]['tris'])

    return props, consumed
