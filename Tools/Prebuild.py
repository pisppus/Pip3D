import os
import sys
import subprocess

Import("env")

try:
    from PIL import Image
except ImportError:
    print("\033[36m[Pip3D]\033[0m Pillow library not found. Installing via pip...")
    python_exe = env.subst("$PYTHONEXE")
    subprocess.check_call([python_exe, "-m", "pip", "install", "Pillow"])

project_dir = env.subst("$PROJECT_DIR")
tools_dir = os.path.join(project_dir, "Tools")

models_dir = os.path.join(tools_dir, "Models")
obj_sources_dir = os.path.join(models_dir, "Sources")
geometry_models_dir = os.path.join(project_dir, "lib", "Pip3D", "Pip3D", "Geometry", "Models")

os.makedirs(geometry_models_dir, exist_ok=True)

if os.path.exists(obj_sources_dir):
    for file in os.listdir(obj_sources_dir):
        if file.endswith(".obj"):
            obj_path = os.path.join(obj_sources_dir, file)
            raw_name = os.path.splitext(file)[0]
            hpp_path = os.path.join(geometry_models_dir, raw_name + ".hpp")
            
            if not os.path.exists(hpp_path) or os.path.getmtime(obj_path) > os.path.getmtime(hpp_path):
                script_path = os.path.join(models_dir, "Convert.py")
                subprocess.check_call([env.subst("$PYTHONEXE"), script_path, obj_path, hpp_path])

textures_dir = os.path.join(tools_dir, "Textures")
tex_sources_dir = os.path.join(textures_dir, "Sources")
display_textures_dir = os.path.join(project_dir, "lib", "Pip3D", "Pip3D", "Rendering", "Display", "Textures")

os.makedirs(display_textures_dir, exist_ok=True)

if os.path.exists(tex_sources_dir):
    for file in os.listdir(tex_sources_dir):
        if file.lower().endswith((".png", ".jpg", ".jpeg")):
            img_path = os.path.join(tex_sources_dir, file)
            raw_name = os.path.splitext(file)[0]
            
            name_parts = raw_name.split('_')
            clean_name = raw_name
            if len(name_parts) > 1:
                try:
                    val = int(name_parts[-1])
                    if val >= 16 and (val & (val - 1)) == 0:
                        clean_name = "_".join(name_parts[:-1])
                except ValueError:
                    pass
                    
            hpp_path = os.path.join(display_textures_dir, clean_name + ".hpp")
            
            if not os.path.exists(hpp_path) or os.path.getmtime(img_path) > os.path.getmtime(hpp_path):
                script_path = os.path.join(textures_dir, "Convert.py")
                subprocess.check_call([env.subst("$PYTHONEXE"), script_path, img_path, hpp_path])