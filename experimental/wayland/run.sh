#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)
: "${WAYLAND_DISPLAY:?A running Wayland session (such as WSLg) is required}"
export MACOBLOX_WAYLAND=1
export MACOBLOX_TRACE_WAYLAND=${MACOBLOX_TRACE_WAYLAND:-1}
export GDK_BACKEND=wayland
# Keep the experiment's prefix and settings separate from normal installs.
export DPREFIX=${DPREFIX:-$HOME/.local/share/macoblox-wayland/darling}
export XDG_CONFIG_HOME=${XDG_CONFIG_HOME:-$HOME/.config/macoblox-wayland}
export XDG_CACHE_HOME=${XDG_CACHE_HOME:-$HOME/.cache/macoblox-wayland}
unset DISPLAY
mkdir -p "$(dirname -- "$DPREFIX")" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME"
cd "$project_dir"
# A separate bus prevents an installed launcher's single instance from
# consuming this checkout's activation and experimental environment.
exec dbus-run-session -- python3 launcher/macoblox-launcher "$@"
