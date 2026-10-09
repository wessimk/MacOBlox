# Native Wayland in WSLg

Development branch: `experimental/native-wayland` in
https://github.com/wessimk/MacOBlox.

From PowerShell, launch this checkout with the native Wayland flag:

```powershell
wsl -d Ubuntu -- bash /mnt/c/Users/alexis-pc/MacOBlox/experimental/wayland/run.sh
```

The script sets `MACOBLOX_WAYLAND=1`, enables Wayland tracing, removes
`DISPLAY`, and starts GTK with `GDK_BACKEND=wayland`. It uses a separate D-Bus
session so another installed launcher cannot consume its activation.
Its default Darling prefix is `~/.local/share/macoblox-wayland/darling`,
settings are under `~/.config/macoblox-wayland/macoblox`, and caches are under
`~/.cache/macoblox-wayland/macoblox`. Explicit environment overrides take
precedence. Keep the renderer set to **OpenGL** for this WSL setup.

Build and run the no-account rendering fixture from the checkout in Ubuntu:

```bash
bash build_debug_shim.sh
python3 tests/run_darling_wayland_render.py --build-dir build \
  --renderer opengl --allow-software --output-dir logs/wsl-wayland-opengl
```

The fixture starts fresh temporary prefixes, verifies native AppKit window
identity and event delivery, and checks pixel readback while switching between
core and compatibility OpenGL contexts on the same view. Its negative control
uses an invalid Vulkan driver to verify two initialization attempts can fail
without leaving the display lock held. Both prefixes are shut down afterward.
`--allow-software` is an explicit fixture option; the default fixture still
requires hardware Zink rendering.

## Verified on October 8, 2026

- Ubuntu 24.04.4, WSL2/WSLg, Mesa 25.2.8 and Clang 18.
- Darling release `v0.1.20260608`, installed from the archive pinned and
  checksum-verified by the upstream installer.
- Built `build/libMacOBloxShims.dylib`, `build/libmacoblox-wayland.so`, and
  the replacement frameworks.
- Native Wayland OpenGL fixture and initialization-failure control passed
  with `DISPLAY` unset. Rendering used llvmpipe software OpenGL.
- Roblox `0.742.0.7421053` reached native Wayland window creation and
  OpenGL initialization during a bounded client startup check.
- Display and graphics Python suites: 38 tests passed after repairing
  fixtures that bypassed the session constructor without initializing its
  cached GPU state.

This setup has not verified GPU acceleration, sign-in, or gameplay. Hardware
Vulkan/Zink initialization failed in this WSL session. The client also logged
shader compatibility errors; fixture success does not establish complete
Roblox presentation. Local evidence is in `logs/wsl-wayland-opengl/`,
`logs/wsl-build.log`, and the `logs/launch-*.log` client logs. These generated
files and the downloaded client are excluded from Git.

### Crash reporter startup fix

The launcher previously killed `RobloxCrashHandler` after a fixed three-second
delay. Under WSL this could interrupt Crashpad's initial pipe handshake and
make Roblox exit with `Failed to initialize crash reporter`. The experimental
branch now keeps the handler alive while Roblox runs and retains scoped
cleanup at session teardown. The failure was reproduced with normal launcher
polling; after the fix, the same startup check remained alive for 45 seconds.

### Main player window remains grey

The real client is not yet a working UI prototype. In the tested WSL session,
Roblox presents one solid grey frame (sampled RGBA `135,135,135,255`) and then
submits no further draws or buffer swaps. The native GL window's
`CGLFlushDrawable` returns success and its Wayland buffer is attached and
committed. The main AppKit event loop remains active. An earlier null drawable
belongs to the layer context; it is distinct from the successfully presented
player context.

A fresh Darling prefix reproduced the same result over 90 seconds. An X11
control also stalled during client startup, so the evidence does not establish
a Wayland-specific cause. Event-loop wakeup, HID registration and alternate
lock experiments did not restore rendering and were removed. The client logs
shader parser and missing-variant errors; their role in the stall is unproven.
The remaining failure is unresolved. Passive CGL tracing now records the first
five flush return codes when `MACOBLOX_TRACE_CGL=1` is enabled. Local evidence
includes `logs/launch-20261008-214014.log` and `logs/wsl-grey-fresh.log`.
