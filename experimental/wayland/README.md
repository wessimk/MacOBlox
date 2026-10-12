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

The initial October 8 checks did not verify GPU acceleration, sign-in, or gameplay.
Hardware Vulkan/Zink initialization failed in this WSL session. The client also logged
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

### Rendering investigation, October 12, 2026

- The 7421053 GLSL pack is now covered by the pinned repair manifest. Its
  Heightmap sources match the already audited 7411056 sources byte for byte;
  only their pack offsets and descriptor indices changed. The four whitespace
  repairs eliminate the uniform parser errors and increase successfully
  compiled shaders from 733 to 735. This does not fix the grey UI.
- A color control reported a complete framebuffer (`GL_FRAMEBUFFER_COMPLETE`)
  and no GL errors. A separate Linux control visibly rendered colored buffers
  through both the parent and child drawable APIs. Directly rendering the
  player to its parent still produced the grey frame; that experiment was
  removed.
- The rendering fixture now requires eight successive background submissions
  to render on the main dispatch queue, with pixel readback on each frame.
  The event wait uses a one-second deadline and delivery must remain below
  250 ms, so a short polling timeout cannot mask a missing dispatch wakeup.
  Validated WSL runs measured maximum queue delays of 0.89–1.34 ms. The Vulkan
  initialization-failure control also passed.
- A full launcher web bridge connected, but the client requested no browser
  page for this window. The grey pixels are in the OpenGL player UI.
- Passive debugger captures showed the first render callback returning and
  clearing its completion guard; the render job was not stepped again during
  the captured interval. A separate ulock control completed 32 cross-thread
  wait/wake exchanges. Neither observation establishes the remaining cause.
- One startup produced UI-colored pixels in its first frame, but subsequent
  runs returned to grey. A fresh cache, focus changes, home navigation and
  background-pausing controls did not establish a repeatable fix. Temporary
  flags and runtime changes were removed or restored after each test.
- Build and 69 process, display, graphics and shader Python checks passed.
  Test players and their crash handlers were stopped after screenshot analysis;
  no test player was left running.

Evidence includes `logs/wsl-final-frame-loop-reviewed/`,
`logs/wsl-ulock-wake-control.log`, and the October 12 client logs. The remaining
UI startup/render-job stall is unresolved; fixture success is not a claim that
the real Roblox UI or gameplay works.

### WSL hardware OpenGL and input, October 12, 2026

WSLg on this installation chose llvmpipe unless `GALLIUM_DRIVER=d3d12` was
explicitly selected. With `/usr/lib/wsl/lib` in `LD_LIBRARY_PATH`, the native
Darling fixture rendered all eight queued frames on `D3D12 (NVIDIA GeForce
RTX 3060)`, without the software-rendering allowance. Maximum queue delay was
1.04 ms. This uses Mesa's OpenGL driver over Direct3D 12; it does not require
Roblox's Metal renderer or Zink.

The launcher now selects this path for OpenGL when `/dev/dxg`, `libd3d12.so`
and `libdxcore.so` are present. Explicit Mesa driver selectors are respected,
existing library search paths are preserved, and both host commands and
already-running Darling guests receive the settings. Bare-metal OpenGL and
the Vulkan selection retain their existing behavior.

Repeated client launches now draw the Create Account / Sign In controls,
but this is still a single-frame, unresponsive UI. Mouse and keyboard traces
show delivery through SDL, the Wayland bridge and AppKit: button events reach
`RBXWindow` and hit-test to `RobloxOgreView`; Tab reaches application event
dispatch. The client produces no second GL frame after those events. The
visible UI occupies approximately 800 by 552 pixels within a larger drawable,
leaving grey margins. Input delivery is verified; interactive UI and correct
layout are unresolved. The window uses the player's OpenGL UI; the web
bridge has only received an attach request.

The WSL prefix contains both audited Darling kqueue repairs and all required
replacement frameworks. CPU-affinity and initial-size-notification controls
did not restore recurring rendering; neither workaround is shipped. Input
tracing is limited to the first 24 button/key transitions and requires
`MACOBLOX_TRACE_WAYLAND`; it does not log typed text or key codes.

Local evidence: `logs/wsl-d3d12-fixture/`, `logs/wsl-runtime-audit.log`,
`logs/launch-20261012-162444.log`, and `logs/launch-20261012-162603.log`.
Every player test was stopped after screenshot analysis.
