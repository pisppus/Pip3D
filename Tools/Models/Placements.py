import sys
sys.dont_write_bytecode = True

from Obj import _tag, _err, sanitize_asset_name, rgb888_to_565

import os


def convert_placements(txt_path, out_path):
    props = []
    scale = 0.01
    rows = []
    with open(txt_path, 'r', encoding='utf-8', errors='ignore') as f:
        for ln, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            if parts[0] == 'props':
                props = [sanitize_asset_name(x) for x in parts[1:]]
                if any(not x for x in props):
                    print(_err(f"Placement line {ln}: bad prop name"))
                    sys.exit(1)
                if len(set(props)) != len(props):
                    print(_err(f"Placement line {ln}: duplicate prop name"))
                    sys.exit(1)
            elif parts[0] == 'scale':
                if len(parts) < 2:
                    print(_err(f"Placement line {ln}: scale needs a number"))
                    sys.exit(1)
                try:
                    scale = float(parts[1])
                except ValueError:
                    print(_err(f"Placement line {ln}: scale '{parts[1]}' is not a number"))
                    sys.exit(1)
                if not scale > 0.0:
                    print(_err(f"Placement line {ln}: scale must be > 0"))
                    sys.exit(1)
            else:
                if len(parts) not in (5, 8):
                    print(_err(f"Placement line {ln}: expected 'prop x y z yaw [r g b]'"))
                    sys.exit(1)
                name = sanitize_asset_name(parts[0])
                if not name:
                    print(_err(f"Placement line {ln}: bad prop name"))
                    sys.exit(1)
                try:
                    x, y, z, yaw = (int(v) for v in parts[1:5])
                except ValueError:
                    print(_err(f"Placement line {ln}: x, y, z and yaw must be integers"))
                    sys.exit(1)
                rgb = parts[5:]
                if rgb:
                    try:
                        rgb = [int(v) for v in rgb]
                    except ValueError:
                        print(_err(f"Placement line {ln}: r g b must be integers"))
                        sys.exit(1)
                rows.append((name, x, y, z, yaw, rgb))

    if not props:
        print(_err(f"Placements: no 'props' line in '{txt_path}'"))
        sys.exit(1)
    if not rows:
        print(_err(f"Placements: no placement rows in '{txt_path}'"))
        sys.exit(1)

    for name, x, y, z, yaw, _rgb in rows:
        if name not in props:
            print(_err(f"Placement line: prop '{name}' is not in the props list"))
            sys.exit(1)
        for v in (x, y, z):
            if not -32768 <= v <= 32767:
                print(_err(f"Placement line: coordinate {v} out of int16"))
                sys.exit(1)
        if not 0 <= yaw <= 255:
            print(_err(f"Placement line: yaw {yaw} out of 0..255"))
            sys.exit(1)

    prop_idx = {name: i for i, name in enumerate(props)}

    os.makedirs(os.path.dirname(out_path) or '.', exist_ok=True)
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("#pragma once\n\n")
        f.write('#include "Geometry/Instance.hpp"\n')
        for name in props:
            f.write('#include "Models/' + name + '.hpp"\n')
        f.write("\nusing namespace pip3D;\n\nnamespace world\n{\n")
        for name in props:
            cls = name[0].upper() + name[1:]
            f.write("    static " + cls + " " + name.lower() + "Mesh;\n")
        f.write("\n    static Mesh *const props[] = {")
        f.write(", ".join("&" + name.lower() + "Mesh" for name in props))
        f.write("};\n\n")
        f.write("    static const Placement placements[" + str(len(rows)) + "] = {\n")
        for name, x, y, z, yaw, rgb in rows:
            if rgb:
                c565 = rgb888_to_565(int(rgb[0]), int(rgb[1]), int(rgb[2]))
            else:
                c565 = 0xFFFF
            f.write("        { " + str(prop_idx[name]) + ", " + str(x) + ", " + str(y) + ", "
                    + str(z) + ", " + str(yaw) + ", kPlacementVisible, Color(0x"
                    + format(c565, '04X') + "u) },\n")
        f.write("    };\n\n")
        f.write("    static const PlacementSet set = {\n")
        f.write("        placements, " + str(len(rows)) + ",\n")
        f.write("        props, " + str(len(props)) + ",\n")
        f.write("        " + str(scale) + "f\n")
        f.write("    };\n}\n")
    print(_tag("Placements: " + txt_path + " -> " + out_path
               + " (" + str(len(rows)) + " placements, " + str(len(props)) + " props, "
               + str(len(rows) * 12) + " bytes table)"))
    return True
