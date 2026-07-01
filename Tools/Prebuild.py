import sys
sys.dont_write_bytecode = True

import os
import json
import hashlib
import subprocess

Import("env")

try:
    from PIL import Image
except ImportError:
    print("\033[36m[Pip3D]\033[0m Pillow not found, installing...")
    subprocess.check_call([env.subst("$PYTHONEXE"), "-m", "pip", "install", "Pillow"])

project_dir = env.subst("$PROJECT_DIR")

ANSI_GREEN  = "\033[32m"
ANSI_YELLOW = "\033[33m"
ANSI_RESET  = "\033[0m"

CACHE_VERSION = 4
CACHE_PATH    = os.path.join(project_dir, ".pio", "prebuild_cache.json")


def _tag(color, msg):
    return f"{color}[Pip3D]{ANSI_RESET} {msg}"


def file_hash(path, chunk=1 << 16):
    h = hashlib.sha256()
    try:
        with open(path, "rb") as f:
            while True:
                buf = f.read(chunk)
                if not buf:
                    break
                h.update(buf)
    except OSError:
        return None
    return h.hexdigest()


def load_cache():
    try:
        with open(CACHE_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
        if data.get("version") != CACHE_VERSION:
            return {}
        return data.get("entries", {})
    except (OSError, ValueError):
        return {}


def save_cache(entries):
    os.makedirs(os.path.dirname(CACHE_PATH), exist_ok=True)
    payload = {"version": CACHE_VERSION, "entries": entries}
    tmp     = CACHE_PATH + ".tmp"
    try:
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)
        os.replace(tmp, CACHE_PATH)
    except OSError:
        pass


def needs_rebuild(entries, key, source_path, output_path):
    src_hash = file_hash(source_path)
    if src_hash is None or not os.path.exists(output_path):
        return True
    rec = entries.get(key)
    if not rec:
        return True
    out_hash = file_hash(output_path)
    if out_hash is None or rec.get("output_hash") != out_hash:
        return True
    return rec.get("source_hash") != src_hash


def mark_built(entries, key, source_path, output_path):
    entries[key] = {
        "source_hash": file_hash(source_path) or "",
        "output_hash": file_hash(output_path) or "",
        "source_file": os.path.basename(source_path),
    }


def touch(path):
    try:
        os.utime(path, None)
    except OSError:
        pass


def parse_asset_name(raw_name):
    name  = os.path.splitext(raw_name)[0]
    parts = name.split("_")
    if len(parts) > 1:
        try:
            val = int(parts[-1])
            if val >= 16 and (val & (val - 1)) == 0:
                return "_".join(parts[:-1]), val
        except ValueError:
            pass
    return name, None


def run_convert(script_path, args):
    env_clean = dict(os.environ)
    env_clean["PYTHONDONTWRITEBYTECODE"] = "1"
    subprocess.check_call(
        [env.subst("$PYTHONEXE"), script_path] + args,
        env=env_clean,
    )


def parse_screen_resolution():
    width  = 480
    height = 320
    try:
        flags_text = env.GetProjectOption("build_flags", "")
        for token in flags_text.replace(",", "\n").split():
            if token.startswith("-DPIP3D_SCREEN_WIDTH="):
                width = int(token.split("=", 1)[1])
            elif token.startswith("-DPIP3D_SCREEN_HEIGHT="):
                height = int(token.split("=", 1)[1])
    except Exception:
        pass
    return width, height


CACHE         = load_cache()
CACHE_CHANGED = False

models_dir          = os.path.join(project_dir, "Tools", "Models")
obj_sources_dir     = os.path.join(models_dir, "Sources")
geometry_models_dir = os.path.join(project_dir, "lib", "Pip3D", "Pip3D", "Geometry", "Models")

os.makedirs(geometry_models_dir, exist_ok=True)

expected_model_outputs = {}

if os.path.isdir(obj_sources_dir):
    for file in os.listdir(obj_sources_dir):
        if file.lower().endswith(".obj"):
            obj_path   = os.path.join(obj_sources_dir, file)
            clean_name, _ = parse_asset_name(file)
            hpp_path   = os.path.join(geometry_models_dir, clean_name + ".hpp")
            expected_model_outputs[os.path.basename(hpp_path)] = True

            key = "model:" + clean_name
            if needs_rebuild(CACHE, key, obj_path, hpp_path):
                print(_tag(ANSI_GREEN, f"Building model: {file} -> {clean_name}.hpp"))
                run_convert(os.path.join(models_dir, "Convert.py"), [obj_path, hpp_path])
                mark_built(CACHE, key, obj_path, hpp_path)
                touch(hpp_path)
                CACHE_CHANGED = True

if os.path.isdir(geometry_models_dir):
    for existing in os.listdir(geometry_models_dir):
        if existing.lower().endswith(".hpp") and existing not in expected_model_outputs:
            stale_path = os.path.join(geometry_models_dir, existing)
            try:
                os.remove(stale_path)
                print(_tag(ANSI_YELLOW, f"Removed orphaned model header: {existing}"))
            except OSError:
                pass

textures_dir         = os.path.join(project_dir, "Tools", "Textures")
tex_sources_dir      = os.path.join(textures_dir, "Sources")
display_textures_dir = os.path.join(project_dir, "lib", "Pip3D", "Pip3D", "Rendering", "Display", "Textures")

os.makedirs(display_textures_dir, exist_ok=True)

expected_tex_outputs = {}

if os.path.isdir(tex_sources_dir):
    claims = {}
    for file in sorted(os.listdir(tex_sources_dir)):
        if file.startswith("_"):
            continue
        if file.lower().endswith((".png", ".jpg", ".jpeg")):
            clean_name, _ = parse_asset_name(file)
            claims.setdefault(clean_name, []).append(file)

    for clean_name, sources in claims.items():
        hpp_path = os.path.join(display_textures_dir, clean_name + ".hpp")
        expected_tex_outputs[os.path.basename(hpp_path)] = True

        if len(sources) > 1:
            chosen   = sorted(sources)[-1]
            img_path = os.path.join(tex_sources_dir, chosen)
            print(_tag(ANSI_YELLOW, f"Name collision for '{clean_name}': {sources} -> using '{chosen}'"))
        else:
            img_path = os.path.join(tex_sources_dir, sources[0])

        key = "tex:" + clean_name
        if needs_rebuild(CACHE, key, img_path, hpp_path):
            print(_tag(ANSI_GREEN, f"Building texture: {sources[-1]} -> {clean_name}.hpp"))
            run_convert(os.path.join(textures_dir, "Convert.py"), [img_path, hpp_path])
            mark_built(CACHE, key, img_path, hpp_path)
            touch(hpp_path)
            CACHE_CHANGED = True

if os.path.isdir(display_textures_dir):
    for existing in os.listdir(display_textures_dir):
        if existing.lower().endswith(".hpp") and existing not in expected_tex_outputs:
            stale_path = os.path.join(display_textures_dir, existing)
            try:
                os.remove(stale_path)
                print(_tag(ANSI_YELLOW, f"Removed orphaned texture header: {existing}"))
            except OSError:
                pass

skygen_path     = os.path.join(project_dir, "Tools", "Textures", "Skygen.py")
clouds_hpp_path = os.path.join(project_dir, "lib", "Pip3D", "Pip3D", "Rendering", "Display", "CloudsMask.hpp")

if os.path.isfile(skygen_path):
    screen_w, screen_h = parse_screen_resolution()
    sky_key = f"skygen:cloudsmask:{screen_w}x{screen_h}"
    if needs_rebuild(CACHE, sky_key, skygen_path, clouds_hpp_path):
        print(_tag(ANSI_GREEN, f"Building cloud mask: Skygen.py -> CloudsMask.hpp ({screen_w}x{screen_h})"))
        run_convert(skygen_path, [clouds_hpp_path, "--screen-w", str(screen_w), "--screen-h", str(screen_h)])
        mark_built(CACHE, sky_key, skygen_path, clouds_hpp_path)
        touch(clouds_hpp_path)
        CACHE_CHANGED = True

if CACHE_CHANGED:
    save_cache(CACHE)