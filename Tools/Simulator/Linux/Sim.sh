#!/usr/bin/env bash
set -euo pipefail

no_run=0
debug=0
clean=0
install_deps=0
jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')"

usage() {
    cat <<'EOF'
usage: Tools/Simulator/Linux/Sim.sh [--no-run] [--debug] [--clean] [--install-deps] [-j jobs]
EOF
}

while (($#)); do
    case "$1" in
        --no-run) no_run=1 ;;
        --debug) debug=1 ;;
        --clean) clean=1 ;;
        --install-deps) install_deps=1 ;;
        -j|--jobs)
            shift
            [[ $# -gt 0 ]] || { usage >&2; exit 2; }
            jobs="$1"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 2
            ;;
    esac
    shift
done

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
resolve_root() {
    local dir="$1"
    while [[ -n "$dir" && "$dir" != "/" ]]; do
        if [[ -f "$dir/platformio.ini" ]] || \
           [[ -d "$dir/include" && -d "$dir/lib/Pip3D" && -d "$dir/Tools/Simulator" ]]; then
            printf '%s\n' "$dir"
            return 0
        fi
        dir="$(dirname -- "$dir")"
    done
    (cd -- "$script_dir/../../.." && pwd)
}

root="$(resolve_root "$script_dir")"
build_dir="$root/.sim"
config_name="$([[ "$debug" -eq 1 ]] && printf debug || printf release)"
cmake_config="Release"
sim_debug="$([[ "$debug" -eq 1 ]] && printf ON || printf OFF)"
cmake_build_dir="$build_dir/build-linux-$config_name"
exe_name="$([[ "$debug" -eq 1 ]] && printf simulator-debug || printf simulator)"
exe="$build_dir/$exe_name"
watchdog_log="$build_dir/sim-watchdog.log"
runtime_log="$build_dir/simulator-runtime.log"

if [[ -t 1 ]]; then
    c_step=$'\033[36m'; c_ok=$'\033[32m'; c_warn=$'\033[33m'; c_fail=$'\033[31m'; c_dim=$'\033[90m'; c_reset=$'\033[0m'
else
    c_step=""; c_ok=""; c_warn=""; c_fail=""; c_dim=""; c_reset=""
fi
step() { printf '  %s>>%s %s\n' "$c_step" "$c_reset" "$*"; }
ok() { printf '  %sOK%s %s\n' "$c_ok" "$c_reset" "$*"; }
warn() { printf '  %s!!%s %s\n' "$c_warn" "$c_reset" "$*" >&2; }
fail() { printf '\n  %sFAIL%s %s\n\n' "$c_fail" "$c_reset" "$*" >&2; exit 1; }

colorize_line() {
    local line="$1"
    if [[ -z "$c_reset" ]]; then
        printf '%s\n' "$line"
        return 0
    fi

    case "$line" in
        "CMake Error:"*|*[[:space:]]"error:"*|*" error:"*)
            printf '%s%s%s\n' "$c_fail" "$line" "$c_reset"
            ;;
        "CMake Warning:"*|*"warning:"*|*" warning:"*|ninja:\ warning:*|ninja:\ miss* )
            printf '%s%s%s\n' "$c_warn" "$line" "$c_reset"
            ;;
        "-- Configuring done"*|"-- Generating done"*|"-- Build files have been written to:"*)
            printf '%s%s%s\n' "$c_ok" "$line" "$c_reset"
            ;;
        "-- "*)
            printf '%s%s%s\n' "$c_step" "$line" "$c_reset"
            ;;
        [[][0-9]*/[0-9]*]*)
            printf '%s%s%s\n' "$c_dim" "$line" "$c_reset"
            ;;
        "Building CXX object"*|"Linking CXX executable"*|"Re-checking globbed directories"*|"Running CMake"*|"Re-running CMake"* )
            printf '%s%s%s\n' "$c_step" "$line" "$c_reset"
            ;;
        "ninja:"*)
            printf '%s%s%s\n' "$c_warn" "$line" "$c_reset"
            ;;
        *)
            printf '%s\n' "$line"
            ;;
    esac
}

run_colored_cmd() {
    local rc=0
    "$@" 2>&1 | while IFS= read -r line || [[ -n "$line" ]]; do
        colorize_line "$line"
    done
    rc=${PIPESTATUS[0]}
    return "$rc"
}

reset_cmake_cache_if_moved() {
    local build_path="$1"
    local source_path="$2"
    local root_path="$3"
    local cache="$build_path/CMakeCache.txt"
    [[ -f "$cache" ]] || return 0

    local expected_source expected_root actual_source actual_root
    expected_source="$(cd -- "$source_path" && pwd)"
    expected_root="$(cd -- "$root_path" && pwd)"
    actual_source="$(awk -F= '/^CMAKE_HOME_DIRECTORY:INTERNAL=/{print $2; exit}' "$cache")"
    actual_root="$(awk -F= '/^SIM_ROOT:[^=]+=/{sub(/^SIM_ROOT:[^=]+=/, ""); print; exit}' "$cache")"

    if [[ -n "$actual_source" && "$actual_source" != "$expected_source" ]] ||
       [[ -n "$actual_root" && "$actual_root" != "$expected_root" ]]; then
        warn "CMake cache belongs to another checkout - rebuilding simulator cache."
        rm -rf -- "$build_path"
        mkdir -p -- "$build_path"
    fi
}

printf '\n  simulator\n'
printf '  %s  jobs=%s  root=%s\n\n' "$config_name" "$jobs" "$root"

if [[ "$install_deps" -eq 1 ]] || ! command -v wx-config >/dev/null 2>&1; then
    "$script_dir/Sim-Deps.sh"
fi

export CMAKE_COLOR_DIAGNOSTICS=ON
command -v cmake >/dev/null 2>&1 || fail "cmake not found. Run Tools/Simulator/Linux/Sim.sh --install-deps or install CMake."
command -v g++ >/dev/null 2>&1 || fail "g++ not found. Run Tools/Simulator/Linux/Sim.sh --install-deps or install GCC/G++."
if ! command -v ffmpeg >/dev/null 2>&1; then
    warn "ffmpeg not found. Recording will show an error until ffmpeg is available."
fi

mkdir -p -- "$build_dir"

if [[ "$clean" -eq 1 ]]; then
    step "Cleaning build outputs..."
    rm -rf -- "$cmake_build_dir"
    rm -rf -- "$build_dir/obj" "$build_dir/obj-release" "$build_dir/cmake-debug" "$build_dir/cmake-release"
    rm -f -- "$exe" "$build_dir/pipgui-sim" "$build_dir/pipgui-sim-debug" "$watchdog_log" "$runtime_log"
    rm -f -- "$build_dir"/*.rsp "$build_dir"/flags*.hash "$build_dir"/gxx.* "$build_dir"/probe.* "$build_dir"/temp.obj "$build_dir"/Runtime.test.obj "$build_dir"/sim-build.*.log
    ok "Clean done."
fi

mkdir -p -- "$cmake_build_dir"

cmake_args=(
    -S "$root/Tools/Simulator"
    -B "$cmake_build_dir"
    -DSIM_ROOT="$root"
    -DSIM_DEBUG="$sim_debug"
    -DCMAKE_BUILD_TYPE="$cmake_config"
)
if command -v ninja >/dev/null 2>&1; then
    cmake_args+=(-G Ninja)
fi

reset_cmake_cache_if_moved "$cmake_build_dir" "$root/Tools/Simulator" "$root"

step "Configuring wxWidgets simulator..."
run_colored_cmd cmake "${cmake_args[@]}"

step "Building wxWidgets simulator..."
run_colored_cmd cmake --build "$cmake_build_dir" --config "$cmake_config" --parallel "$jobs"

ok "done -> $exe"
printf '\n'

if [[ "$no_run" -eq 0 ]]; then
    step "launching..."
    (
        if [[ -z "${PIPGUI_SIM_THEME:-}" ]]; then
            scheme="$(gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null || true)"
            gtk_pref="$(gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null || true)"
            xfce_theme="$(xfconf-query -c xsettings -p /Net/ThemeName 2>/dev/null || true)"
            gtk_theme="${GTK_THEME:-}"
            if [[ "$scheme" == *dark* || "$gtk_pref" == *dark* || "$gtk_pref" == *Dark* ||
                  "$xfce_theme" == *dark* || "$xfce_theme" == *Dark* ||
                  "$gtk_theme" == *dark* || "$gtk_theme" == *Dark* ]]; then
                export PIPGUI_SIM_THEME=dark
            fi
        fi
        export PIPGUI_SIM_WORKDIR="$build_dir"
        export PIPGUI_SIM_LAST_EXIT=""
        user_exit="$build_dir/_sim-user-exit"
        restart_request="$build_dir/_sim-restart"
        rm -f -- "$user_exit" "$restart_request"
        while true; do
            set +e
            if command -v python3 >/dev/null 2>&1; then
                (cd "$build_dir" && python3 - "$exe" "$runtime_log" <<'PY'
import subprocess
import sys

exe = sys.argv[1]
log_path = sys.argv[2]
with open(log_path, "ab", buffering=0) as log:
    proc = subprocess.run([exe], stdout=log, stderr=log)
sys.exit(proc.returncode)
PY
                )
            else
                (cd "$build_dir" && "$exe" >>"$runtime_log" 2>&1)
            fi
            rc=$?
            set -e
            if [[ -f "$user_exit" ]]; then
                rm -f -- "$user_exit"
                break
            fi
            if [[ -f "$restart_request" ]]; then
                rm -f -- "$restart_request"
                sleep 0.15
                continue
            fi
            [[ "$rc" -eq 0 ]] && break
            msg="previous simulator exited with code $rc; restarted at $(date '+%Y-%m-%d %H:%M:%S')"
            printf '%s\n' "$msg" >>"$watchdog_log"
            export PIPGUI_SIM_LAST_EXIT="$msg"
            warn "$msg"
            sleep 0.5
        done
    ) &
fi
