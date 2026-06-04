#!/usr/bin/env bash
set -euo pipefail

if [[ -t 1 ]]; then
    c_ok=$'\033[32m'; c_fail=$'\033[31m'; c_reset=$'\033[0m'
else
    c_ok=""; c_fail=""; c_reset=""
fi

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        printf '\n  %sFAIL%s %s not found\n\n' "$c_fail" "$c_reset" "$1" >&2
        exit 1
    }
}

detect_pm() {
    if command -v apt-get >/dev/null 2>&1; then echo apt; return; fi
    if command -v dnf >/dev/null 2>&1; then echo dnf; return; fi
    if command -v pacman >/dev/null 2>&1; then echo pacman; return; fi
    if command -v zypper >/dev/null 2>&1; then echo zypper; return; fi
    echo ""
}

sudo_cmd=()
if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
    need_cmd sudo
    sudo_cmd=(sudo)
fi

pm="$(detect_pm)"
case "$pm" in
    apt)
        "${sudo_cmd[@]}" apt-get update
        "${sudo_cmd[@]}" apt-get install -y build-essential cmake ninja-build pkg-config ffmpeg libwxgtk3.2-dev
        ;;
    dnf)
        "${sudo_cmd[@]}" dnf install -y gcc-c++ cmake ninja-build pkgconf-pkg-config ffmpeg wxGTK-devel
        ;;
    pacman)
        "${sudo_cmd[@]}" pacman -Syu --needed base-devel cmake ninja pkgconf ffmpeg wxwidgets-gtk3
        ;;
    zypper)
        "${sudo_cmd[@]}" zypper install -y gcc-c++ cmake ninja pkgconf-pkg-config ffmpeg wxGTK3-devel
        ;;
    *)
        printf '\n  %sFAIL%s supported package manager not found. Install CMake, GCC, ffmpeg, and wxWidgets development packages manually.\n\n' "$c_fail" "$c_reset" >&2
        exit 1
        ;;
esac

printf '  %sOK%s Simulator dependencies are ready.\n' "$c_ok" "$c_reset"
