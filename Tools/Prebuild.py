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

try:
    import miniaudio
    import numpy
    import matplotlib
except ImportError:
    print("\033[36m[Pip3D]\033[0m Audio tools (miniaudio/numpy/matplotlib) not found, installing...")
    subprocess.check_call([env.subst("$PYTHONEXE"), "-m", "pip", "install", "miniaudio", "numpy", "matplotlib"])

project_dir = env.subst("$PROJECT_DIR")

ANSI_GREEN  = "\033[32m"
ANSI_YELLOW = "\033[33m"
ANSI_RESET  = "\033[0m"

CACHE_VERSION = 2
CACHE_PATH    = os.path.join(project_dir, ".pio", "prebuild_cache.json")

CHUNK_SUFFIX            = "_chunk"
BUILTIN_ENGINE_MODELS   = {"suzanne", "teapot"}
BUILTIN_ENGINE_TEXTURES = {"barrier", "concrete", "gravel", "sun", "tile"}


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


def parse_model_asset_name(raw_name):
    name = os.path.splitext(raw_name)[0]
    if name.lower().endswith(CHUNK_SUFFIX):
        clean = name[:-len(CHUNK_SUFFIX)]
        return clean if clean else name
    return name


def parse_tex_asset_name(raw_name):
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

models_dir        = os.path.join(project_dir, "Tools", "Models")
obj_sources_dir   = os.path.join(models_dir, "Sources")

engine_models_dir = os.path.join(project_dir, "lib", "Pip3D", "Pip3D", "Geometry", "Models")
app_models_dir    = os.path.join(project_dir, "src", "Models")

os.makedirs(engine_models_dir, exist_ok=True)
os.makedirs(app_models_dir, exist_ok=True)

expected_engine_models = {}
expected_app_models    = {}

if os.path.isdir(obj_sources_dir):
    for file in os.listdir(obj_sources_dir):
        if file.lower().endswith(".obj"):
            obj_path   = os.path.join(obj_sources_dir, file)
            clean_name = parse_model_asset_name(file)

            is_engine = clean_name.lower() in BUILTIN_ENGINE_MODELS
            dest_dir  = engine_models_dir if is_engine else app_models_dir
            hpp_path  = os.path.join(dest_dir, clean_name + ".hpp")

            if is_engine:
                expected_engine_models[os.path.basename(hpp_path)] = True
            else:
                expected_app_models[os.path.basename(hpp_path)] = True

            key = "model:" + file
            if needs_rebuild(CACHE, key, obj_path, hpp_path):
                dest_label = "Engine (Geometry/Models)" if is_engine else "App (src/Models)"
                print(_tag(ANSI_GREEN, f"Building model: {file} -> {dest_label}/{clean_name}.hpp"))
                run_convert(os.path.join(models_dir, "Convert.py"), [obj_path, hpp_path])
                mark_built(CACHE, key, obj_path, hpp_path)
                touch(hpp_path)
                CACHE_CHANGED = True

if os.path.isdir(engine_models_dir):
    for existing in os.listdir(engine_models_dir):
        if existing.lower().endswith(".hpp"):
            base_name = os.path.splitext(existing)[0].lower()
            if base_name not in BUILTIN_ENGINE_MODELS or existing not in expected_engine_models:
                stale_path = os.path.join(engine_models_dir, existing)
                try:
                    os.remove(stale_path)
                    print(_tag(ANSI_YELLOW, f"Removed non-engine model from lib/Pip3D: {existing}"))
                except OSError:
                    pass

if os.path.isdir(app_models_dir):
    for existing in os.listdir(app_models_dir):
        if existing.lower().endswith(".hpp") and existing not in expected_app_models:
            stale_path = os.path.join(app_models_dir, existing)
            try:
                os.remove(stale_path)
                print(_tag(ANSI_YELLOW, f"Removed orphaned app model header: {existing}"))
            except OSError:
                pass


textures_dir          = os.path.join(project_dir, "Tools", "Textures")
tex_sources_dir       = os.path.join(textures_dir, "Sources")

engine_textures_dir   = os.path.join(project_dir, "lib", "Pip3D", "Pip3D", "Rendering", "Resources", "Textures")
app_textures_dir      = os.path.join(project_dir, "src", "Textures")

os.makedirs(engine_textures_dir, exist_ok=True)
os.makedirs(app_textures_dir, exist_ok=True)

expected_engine_textures = {}
expected_app_textures    = {}

if os.path.isdir(tex_sources_dir):
    claims = {}
    for file in sorted(os.listdir(tex_sources_dir)):
        if file.startswith("_"):
            continue
        if file.lower().endswith((".png", ".jpg", ".jpeg")):
            clean_name, _ = parse_tex_asset_name(file)
            claims.setdefault(clean_name, []).append(file)

    for clean_name, sources in claims.items():
        is_engine = clean_name.lower() in BUILTIN_ENGINE_TEXTURES
        dest_dir  = engine_textures_dir if is_engine else app_textures_dir
        hpp_path  = os.path.join(dest_dir, clean_name + ".hpp")

        if is_engine:
            expected_engine_textures[os.path.basename(hpp_path)] = True
        else:
            expected_app_textures[os.path.basename(hpp_path)] = True

        if len(sources) > 1:
            chosen   = sorted(sources)[-1]
            img_path = os.path.join(tex_sources_dir, chosen)
            print(_tag(ANSI_YELLOW, f"Name collision for '{clean_name}': {sources} -> using '{chosen}'"))
        else:
            img_path = os.path.join(tex_sources_dir, sources[0])

        key = "tex:" + clean_name
        if needs_rebuild(CACHE, key, img_path, hpp_path):
            dest_label = "Engine (Resources/Textures)" if is_engine else "App (src/Textures)"
            print(_tag(ANSI_GREEN, f"Building texture: {sources[-1]} -> {dest_label}/{clean_name}.hpp"))
            run_convert(os.path.join(textures_dir, "Convert.py"), [img_path, hpp_path])
            mark_built(CACHE, key, img_path, hpp_path)
            touch(hpp_path)
            CACHE_CHANGED = True

sun_hpp_path = os.path.join(engine_textures_dir, "Sun.hpp")
sungen_path  = os.path.join(textures_dir, "Sungen.py")
if os.path.isfile(sungen_path):
    expected_engine_textures["Sun.hpp"] = True
    sun_key = "sungen:sun"
    if needs_rebuild(CACHE, sun_key, sungen_path, sun_hpp_path):
        print(_tag(ANSI_GREEN, f"Building sun texture: Sungen.py -> Sun.hpp"))
        run_convert(sungen_path, [sun_hpp_path])
        mark_built(CACHE, sun_key, sungen_path, sun_hpp_path)
        touch(sun_hpp_path)
        CACHE_CHANGED = True

if os.path.isdir(engine_textures_dir):
    for existing in os.listdir(engine_textures_dir):
        if existing.lower().endswith(".hpp"):
            base_name = os.path.splitext(existing)[0].lower()
            if base_name not in BUILTIN_ENGINE_TEXTURES or existing not in expected_engine_textures:
                stale_path = os.path.join(engine_textures_dir, existing)
                try:
                    os.remove(stale_path)
                    print(_tag(ANSI_YELLOW, f"Removed non-engine texture from lib/Pip3D: {existing}"))
                except OSError:
                    pass

if os.path.isdir(app_textures_dir):
    for existing in os.listdir(app_textures_dir):
        if existing.lower().endswith(".hpp") and existing not in expected_app_textures:
            stale_path = os.path.join(app_textures_dir, existing)
            try:
                os.remove(stale_path)
                print(_tag(ANSI_YELLOW, f"Removed orphaned app texture header: {existing}"))
            except OSError:
                pass


skygen_path     = os.path.join(project_dir, "Tools", "Textures", "Skygen.py")
clouds_hpp_path = os.path.join(project_dir, "lib", "Pip3D", "Pip3D", "Rendering", "Environment", "CloudsData.hpp")

if os.path.isfile(skygen_path):
    screen_w, screen_h = parse_screen_resolution()
    sky_key = f"skygen:cloudsdata:{screen_w}x{screen_h}"
    if needs_rebuild(CACHE, sky_key, skygen_path, clouds_hpp_path):
        print(_tag(ANSI_GREEN, f"Building cloud data: Skygen.py -> CloudsData.hpp ({screen_w}x{screen_h})"))
        run_convert(skygen_path, [clouds_hpp_path, "--screen-w", str(screen_w), "--screen-h", str(screen_h)])
        mark_built(CACHE, sky_key, skygen_path, clouds_hpp_path)
        touch(clouds_hpp_path)
        CACHE_CHANGED = True


audio_dir          = os.path.join(project_dir, "Tools", "Audio")
audio_sources_dir  = os.path.join(audio_dir, "Sources")
sounds_output_dir  = os.path.join(project_dir, "lib", "Pip3D", "Pip3D", "Audio", "Sounds")

os.makedirs(sounds_output_dir, exist_ok=True)
expected_audio_outputs = {}

if os.path.isdir(audio_sources_dir):
    audio_claims = {}
    for file in sorted(os.listdir(audio_sources_dir)):
        if file.startswith("_"):
            continue
        if file.lower().endswith((".wav", ".mp3", ".ogg", ".flac")):
            clean_name, _ = parse_tex_asset_name(file)
            audio_claims.setdefault(clean_name, []).append(file)

    for clean_name, sources in audio_claims.items():
        hpp_path = os.path.join(sounds_output_dir, clean_name + ".hpp")
        expected_audio_outputs[os.path.basename(hpp_path)] = True

        if len(sources) > 1:
            chosen = sorted(sources)[-1]
            src_path = os.path.join(audio_sources_dir, chosen)
            print(_tag(ANSI_YELLOW, f"Name collision for audio '{clean_name}': {sources} -> using '{chosen}'"))
        else:
            src_path = os.path.join(audio_sources_dir, sources[0])

        key = "audio:" + clean_name
        if needs_rebuild(CACHE, key, src_path, hpp_path):
            print(_tag(ANSI_GREEN, f"Building audio: {sources[-1]} -> {clean_name}.hpp"))
            run_convert(os.path.join(audio_dir, "Convert.py"), [src_path, hpp_path])
            mark_built(CACHE, key, src_path, hpp_path)
            touch(hpp_path)
            CACHE_CHANGED = True

if os.path.isdir(sounds_output_dir):
    for existing in os.listdir(sounds_output_dir):
        if existing.lower().endswith(".hpp") and existing not in expected_audio_outputs:
            stale_path = os.path.join(sounds_output_dir, existing)
            try:
                os.remove(stale_path)
                print(_tag(ANSI_YELLOW, f"Removed orphaned audio header: {existing}"))
            except OSError:
                pass

if CACHE_CHANGED:
    save_cache(CACHE)