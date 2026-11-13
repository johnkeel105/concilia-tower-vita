# Vita Tower on PS Vita (native GXM)

*(Formerly "Concilia Tower"; renamed in v0.10. Title ID CTWR00001 and save folder `ux0:data/concilia-tower/` are unchanged.)*

This tree can be built as a PS Vita `.vpk` using VitaSDK and **sdl2_vitagl**
(SDL2 Vita backend); the game selects the native GXM renderer, not vitaGL.

The VPK is the engine only. It does **not** ship `SIMTOWER.EXE`, CD images,
or any other Maxis assets, and it does **not** ship `libshacccg.suprx`.

## Build

```sh
export VITASDK=/path/to/vitasdk
export PATH="$VITASDK/bin:$PATH"
# one-time packages: sdl2_vitagl sdl2_ttf (pulls freetype, harfbuzz, libpng, zlib, bzip2)
vdpm install sdl2_vitagl sdl2_ttf

cmake -S . -B build-vita -DCMAKE_BUILD_TYPE=Release   -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
cmake --build build-vita -j$(nproc)
```

The package is `build-vita/concilia-tower.vpk`.

Or: `make vita` (same steps).

## Install on a Vita

1. Install the VPK with VitaShell (Title ID `CTWR00001`).
2. Copy a legally obtained `SIMTOWER.EXE` to:

   `ux0:data/concilia-tower/SIMTOWER.EXE`

   Original CD/floppy media often ship `SIMTOWER.EX_` instead of a plain
   EXE. Drop that file in the same folder and launch — the port will expand
   it to `SIMTOWER.EXE` on first run.
3. If the EXE is missing, the app shows an on-screen path reminder and waits
   for START/Circle rather than silently exiting.

The default save remains `ux0:data/concilia-tower/concilliatower.sav`.
Game > Save As offers numbered `tower-01.sav` ... `tower-16.sav` slots and
existing `.sav` files; Game > Load lists that same userdata folder. Save uses
the selected slot for the session. Autosave/exports also stay in userdata.
`--save`, `CT_SAVE`, cwd and `app0:` cannot redirect Vita saves.
Save/load paths and results are flushed to `ux0:data/concilia-tower/boot.log`.

Native `.sav` files are architecture-specific (the Vita build is 32-bit, so
they are not interchangeable with desktop 64-bit saves). Use original
`.TDT`/`.TWR` files to move towers between the Vita port, the desktop/web
builds, and SimTower 1.1.

## Controls

| Vita | Action |
| --- | --- |
| Front touch | Move pointer; tap/click and drag |
| Left analog | Camera |
| Right analog | Visible mouse cursor |
| D-pad | Fine cursor nudge |
| Cross / R trigger | Left click / hold to drag (combined safely with touch) |
| L trigger | Right click / camera drag |
| Circle | Select Hand / pointer tool |
| Triangle | Select Inspect / magnifying glass tool |
| Square | Select Bulldozer tool |
| Start | Game menu |
| Select | Debug labels |

## Runtime notes (native GXM)

Since v0.5 the renderer uses native GXM with precompiled shaders. It does not
initialize vitaGL or need `libshacccg.suprx`. The SDK still links sdl2_vitagl,
but this game uses only SDL's renderer API and chooses its native GXM driver.
A homebrew-capable console is required. Do not include the Sony compiler or
SimTower assets in the VPK or repository.

## Legal

Same rule as the desktop/web builds: supply your own `SIMTOWER.EXE`.
SimTower is © its rights holders. This project is not affiliated with Maxis,
OpenBook, or Vivarium.


## v0.3 boot diagnostics (APP_VER 00.03)

This build recognizes KWAJ-compressed media **even when renamed SIMTOWER.EXE**.
It displays decompression progress using a direct framebuffer (no shaders/TTF),
writes a temporary expanded file, validates its MZ/NE headers, and only then
installs `ux0:data/concilia-tower/SIMTOWER.EXE`. An existing compressed destination
is preserved as `SIMTOWER.EXE.kwaj.bak` (or a numbered backup if that exists).
Allow at least 10 MB of free space. Do not power off during expansion.
Uncompressed valid EXEs are left untouched.

Startup diagnostics: `ux0:data/concilia-tower/boot.log` (every record flushed,
closed and synced). The previous launch is retained in `boot.previous.log`.
The log includes file probes/sizes, MZ/NE validation, resource types, clocks,
font loading, SDL/window/context setup, vitaGL's boolean init result, SDL shader
compilation/link results and the first gameplay frame. Presence of the shader
module alone does not prove it can be loaded. Build version is logged as v0.3.

Recoverable startup errors stop at a direct-framebuffer text screen and wait
for START or Circle (release buttons first); they no longer silently return from
main. Send the entire `boot.log`, plus a photo of any message. If the system
still terminates the process, the last completed log record narrows the failing
stage. This is not a CPU exception handler: arbitrary native crashes cannot be
recovered safely, and a last record does not prove the following step completed.
If no log appears, report that as well (the process may have failed before main
or the data directory could not be written).

The SDK's `share/gcc-arm-vita-eabi/samples/common/debugScreen.c` and bitmap font
are compiled for the error/progress display. No proprietary shader library,
SimTower executable, decompressed resources or emulator data goes into the VPK.
LiveArea images remain 8-bit indexed PNG.

### Findings / validation

* Misnamed KWAJ EXE explains the old immediate exit: NE parser rejects its missing
  MZ header and main used to return 1, before SDL was initialized.
* v0.3 incorrectly interpreted `vglInitExtended`'s boolean as success/failure.
  This is corrected in v0.4 as described below; zero is normal initialization.
* SDL GLES2 renderer/context is explicitly requested; unsupported renderer or
  texture upload failures stop visibly instead of proceeding with null objects.
* CPU heap is explicitly 192 MiB (SDK default was 128 MiB). This is headroom,
  **not** a confirmed cause. vitaGL retains separate pools; parameters are logged.
* Parser now uses the resource table's alignment word (not segment alignment),
  checks allocations and unsigned bounds, and rejects short reads. DIB reads
  avoid ARM unaligned casts and check pixel bounds. Texture readback failures no
  longer return uninitialized pixels.
* `-fstack-usage` measured startup main at under 1 KiB; largest game C frame was
  the audio mixer at about 16 KiB. Static BSS is about 5 MiB. No evidence justified
  a speculative main-thread stack change or 64-bit compatibility rewrite.

Host-only parser smoke harness (no SDL development packages required):

```sh
gcc -std=gnu11 -g -O1 -fsanitize=address,undefined -Isrc -Isrc/vendor/mspack \
  tests/test_exe_boot.c src/exe_prepare.c src/ne_resource.c src/kwaj_expand.c \
  src/vendor/mspack/{kwajd,mszipd,lzssd,system}.c -o /tmp/ct-exe-boot-test
/tmp/ct-exe-boot-test /path/to/your/EXE /tmp/expanded.exe
```

Tested on the host: supplied expanded SimTower and supplied misnamed KWAJ both
parse into 20 resource types / 499 resources; expanded bytes match SHA-256
`2825a3c53f77945c63b6d72e26faa7dde5ddd56c31ca668e67a12576d7feca96`.
Original backup/relaunch behavior, malformed input, truncated KWAJ and mismatched
NE segment/resource alignment were also tested with ASan/UBSan. Native Vita
rendering/controls still require hardware testing; this does not claim a verified
Vita3K boot.


## v0.4: spinner / initialization correction (APP_VER 00.04)

The real-device v0.3 log established successful EXE expansion, resource parsing,
SDL video startup and vitaGL pool allocation. Our v0.3 interposer then incorrectly
called a zero vitaGL return a failure. The exact installed SDK package is
`vitaGL-0.0.0.r1488.g2bdbe89-1`; its source declares GLboolean but returns
`res_fallback` (whether the requested framebuffer resolution was clamped), NOT
an initialization-success flag. **Zero is the normal 960x544 result.**
Reference: <https://github.com/Rinnegatamante/vitaGL/blob/2bdbe89/source/vgl.c>.
The linked binary's duplicate-init guard and init call graph were also inspected.

There was no explicit second initialization in our startup: SDL owns its single
call through our interposer. v0.4 keeps that path and guards duplicate requests
without calling into vitaGL a second time. After SDL creates its renderer, we
validate the actual current SDL GL context, allocated vitaGL pool, GL_VERSION,
GL_RENDERER and GL_VENDOR, then exercise the renderer's first clear/present.
The reported `Unexpected controller element crc` was a stale SDL error, not a
vitaGL failure. SDL error state is cleared before relevant operations; any fatal
snapshot is explicitly labeled potentially stale rather than called the cause.

v0.3 tried to allocate a diagnostic framebuffer only AFTER vitaGL reserved all
CDRAM, which failed with `0x80024309` and left the splash displayed. v0.4 reserves
2 MiB for the SDK bitmap-font framebuffer BEFORE vitaGL starts, retains it while
SDL renders, and restores it without allocating or compiling shaders on error.
Queued GL frames cannot permanently cover it: the error wait reasserts the saved
framebuffer. In the exceptional case that even that display cannot be restored,
the invisible wait exits after 30 seconds instead of hanging indefinitely.
No unsupported `vglEnd` symbol is assumed; this SDK has no such public function.

Memory tuning: leave 24 MiB of user RAM outside vitaGL (previously 12 MiB), and
set the 2D circular vertex stream to 4 MiB (previously 32 MiB). CPU heap stays
192 MiB. The reserved framebuffer protects CDRAM even if other pools consume the
remaining memory. Pool allocations/free memory are logged.

A separate boot watchdog records a stage timeout after 60 seconds and every
30 seconds thereafter. It does NOT touch GL or force-reset a stalled thread.
It stops at the first gameplay frame or an acknowledged error path. If still
stuck, wait at least 65 seconds, then send `boot.log`; it should name the stalled
stage. A fresh real-Vita test is still required; successful compilation is not
hardware validation.

LiveArea: background title/subtitle moved to y=130/182 below the system's round
icons; startup gate now has one-line ConciliaTower and readable PS Vita labels,
with its bottom 40 pixels clear for the system Start control. All artwork remains
8-bit indexed PNG. Reproduce with `python3 tools/livearea_art.py` (Pillow).


## v0.5: native GXM; no runtime video shader compilation (APP_VER 00.05)

The exact installed `sdl2_vitagl-2.32.8-1` builds the standard SDL 2.32.8 renderer
with `vitasdk/packages/sdl2_vitagl/vitagl-backend.patch`. Crucially, that patch
replaces the GLES2 GLSL table with **Cg shaders**. The first six successful shaders
are vertex, solid, ABGR, ARGB, BGR, RGB. **Shader object #7 is table type 6,
`GLES2_SHADER_FRAGMENT_TEXTURE_YUV_JPEG`**, the first planar video conversion
shader. It is not a shader written by ConciliaTower, and not external-OES/NV12.
This identification was checked against symbols and bytes in the installed
`libSDL2.a`'s `SDL_shaders_gles2_cg.c.obj`, not just a generic upstream table.
Its Cg function contains `uniform vec4 u_modulation` and a trailing parameter
comma: malformed Cg. Other YUV variants have additional GLSL remnants. The
compiler supplied no diagnostic string, so the specific rejected token was not
reported by hardware. SDL eagerly compiles this unused table during renderer
construction, and v0.4's per-compile fatal hook stopped even its retry handling.

References:
- <https://github.com/vitasdk/packages/blob/master/sdl2_vitagl/VITABUILD>
- <https://github.com/vitasdk/packages/blob/master/sdl2_vitagl/vitagl-backend.patch>
- <https://github.com/libsdl-org/SDL/blob/release-2.32.8/src/render/opengles2/SDL_render_gles2.c>

**v0.5 selects SDL's native `VITA gxm` renderer by name from the start.** No
OPENGL window, no GL context attributes, no vitaGL initialization, no speculative
GL-then-GXM switch, and no runtime `libshacccg` requirement. Game drawing uses
SDL_Renderer throughout; all raw GL calls were diagnostic hooks, not gameplay.
Native GXM uses SDL's built-in precompiled shader programs. This skips all of the
unused GLES2 YUV/NV12/external shaders rather than lying about compile success.
If native GXM is unavailable or fails, we stop visibly; we do not mix two GXM
owners/pools after a failed GL attempt (the installed vitaGL has no safe teardown).
The exact selected renderer and zero vitaGL-init calls are logged.

A necessary compatibility fix: native GXM's SDL_RenderReadPixels does not support
render-target textures. Sprite keys and building composites formerly depended on
that operation, so sprites now retain their CPU source surfaces and compose/key
on CPU before uploading. All replacements update texture and CPU source
atomically, and cleanup frees both. CPU source atlas is about 15.4 MB for the
supplied EXE's 261 textures, fitting the existing 192 MiB CPU heap. This prevents
missing composites/transparency on the native backend. Render-target drawing for
other UI effects is retained; screenshot readback only targets the main display.

Diagnostics now log the **exact historical #7 source from the installed SDL
selector at boot**, even though it is never compiled in the native path. If GL
is ever used during future debugging, hooks retain glShaderSource text and dump
it plus glGetShaderSource, GL_INFO_LOG_LENGTH and glGetShaderInfoLog on failure.
A compile/link failure is returned to SDL rather than treated as globally fatal
before SDL can retry or report renderer-level failure.

Host ASan/UBSan tests cover surface replacement, white transparency, horizontal
and vertical composition, transparent padding and cleanup, including all 261
real EXE textures and a representative building composite. Fixed signed alpha
left shifts found by UBSan. Cross-build and package checks pass. This is not yet
a real-Vita gameplay validation; send `boot.log` if a later stage fails.


## v0.6: renderer-bounded logical textures (APP_VER 00.06)

Hardware reached native GXM, fonts and its first blank frame in v0.5. The two
remaining failed uploads were raw animation strips **4416x36 and 5888x36**,
exceeding GXM's advertised **4096x4096** maximum. They were not missing assets.

`src/texture.c` now owns a typed logical texture abstraction used by **every
sprite, palette-cycle frame, keyed replacement, composite, UI image and font
texture**, including the direct draw calls in main.c. It queries actual
SDL_RendererInfo limits and tiles oversized images on either axis, retaining
original logical dimensions and source coordinates. A frame spanning a tile
boundary draws the appropriate pieces; color/alpha/blend/scale settings and
cleanup apply to every tile. Images within the limit keep SDL's direct path.
Fractionally scaled cross-tile draws use a cached CPU crop (resampled only if
the source crop itself exceeds the limit) to avoid
SDL software-renderer rounding/phase seams; ordinary contained sprite frames
stay on the direct GPU path. Extra CPU sources are retained only for oversized
textures. An oversized optional viewport render target is safely disabled
rather than pretending a partial target represents a whole image. The Vita's
normal 960x544 target is within the cap.

A failed cosmetic upload logs dimensions/SDL error and retains its logical
size as a visible magenta placeholder. A single asset no longer makes the
atlas fatal. Failed optional replacements keep the previous working image.
No valid sprite assets remains a visible startup error, as before.

Later-stage review/hardening:
- Audio is initialized separately from critical video/controller startup, and
  device/decode failures leave the game running silently rather than fatal.
- Logs include device format, each WAV decode, PCM total and completion.
  Stream conversion/get failures are checked rather than mixing uninitialized
  data. All 58 supplied sounds decode/mix on the host (7,384,092 PCM bytes).
- SDL worker thread stack request is 128 KiB before opening the audio device;
  the mixer's 16 KiB automatic accumulator plus resampler needs headroom.
  Compiler stack reports show a largest game function frame of 16,416 bytes
  (mixer), tower drawing 8,968 bytes, main 920 bytes; the existing Vita main
  stack/192 MiB heap remain. Binary BSS is about 4.8 MiB, not a giant stack.
- First gameplay world drawing, UI drawing and actual presentation have
  distinct watchdog/log stages. Shutdown stops audio before freeing samples
  and releases all logical/UI/cached textures before destroying the renderer.

ASan/UBSan host tests:
- `tests/test_texture_tiles.c`: force caps **4096 and 1024**, compare tiled draws
  pixel-for-pixel against an unbounded SDL reference. Includes both real strip
  dimensions, tall and 2-D images, source-boundary crossings, integer/fractional
  scaling, clipping, alpha/color modulation, keyed transparency, RGBA alpha,
  injected upload failure/placeholder and cleanup.
- `tests/test_sprite_cpu.c EXE LIMIT`: all **261 real textures** load without a
  placeholder at both caps; CPU composition/key tests pass. An injected single
  failed upload preserves a usable atlas rather than failing initialization.
- `tests/test_audio_startup.c EXE`: all 58 clips decode and mix offline.
Compile each with SDL2 headers/libs, `-Isrc -fsanitize=address,undefined`; texture
suite links src/texture.c; sprite suite also links src/sprites.c and
src/ne_resource.c; audio suite links src/audio.c and src/ne_resource.c.
No copyrighted EXE/resource fixture is committed or packaged.

Menu TITLE and launch-gate title are **Concilia Tower**. Backdrop is artwork
only (no title/subtitle overlay), while the centered launch-gate text remains.
All three PNGs stay 8-bit indexed. Release: `ConciliaTower-v0.6.vpk`.
Cross-build/package/host tests pass; real-Vita gameplay validation is still
needed. If it stops later, send `ux0:data/concilia-tower/boot.log` again.


## v0.7: usable Vita pointer/touch and a 60 Hz render target (APP_VER 00.07)

v0.6 reached gameplay on hardware. Its controls had both analog sticks wired to
camera movement, Cross/Circle to Return/Escape, Start to pause, and relied on a
system cursor/touch-generated mouse that the native backend did not provide.

Controls (latest requested mapping):
- **Left stick**: camera scroll, elapsed-time based, with deadzone.
- **Right stick**: visible virtual mouse, deadzone and accelerated response.
- **D-pad**: 2-pixel fine cursor nudge, then slow repeat while held.
- **Cross**: left button down/up; hold with cursor movement for dragging.
- **Circle**: right button down/up (the game's right-drag camera behavior).
- **Start**: open/close the File menu, not pause. L/R change simulation speed,
  Triangle saves, Select toggles debug labels as before.
- **Front touch**: tap/click or drag, moving the same visible cursor. One primary
  finger owns a gesture; rear/multiple extra contacts do not create clicks.

The pointer is a one-time-uploaded outlined arrow, drawn above every game/modal
layer rather than an unsupported system cursor. It starts visible, clamps to
0..959 / 0..543, and shares the fixed 960x544 SDL logical render space. Mouse
coordinates/grid hit tests refresh before modal/button handling and after camera
movement; stationary-pointer clicks after left-stick scrolling no longer use a
stale world cell. Focus loss cancels pressed buttons/drags. Touch plus Cross
aggregate one left-button state so either cannot prematurely release the other.
Tap/click sequences also provide double-click counts.

Installed SDL backend inspection (`SDL_vitatouch.c.obj`) confirms front device
ID 1, rear ID 2, normalized finger coordinates, environment gates
VITA_DISABLE_TOUCH_FRONT/BACK, and a private Vita_Window poll guard. v0.7 enables
front sampling, disables rear polling, disables SDL's touch-to-mouse and
mouse-to-touch synthesis, and translates front FINGER events explicitly. It
ignores SDL_TOUCH_MOUSEID only to suppress duplicate synthetic clicks. If a real
front contact is visible through sceTouchPeek but produces no SDL finger event,
SDK front polling becomes the single authoritative touch source for that run;
its panel active area is normalized through exactly the same event core.
The selected source and first touch are logged.

Performance findings/fixes (target is 60 fps; not yet hardware measured):
- Removed the unconditional 16 ms sleep **after** vsynced presentation, which
  could turn a 60 Hz native renderer into roughly 30 fps. Only a remaining
  frame-budget delay is added when presentation/work consumed less than 1/60 s.
- Previously every text label was rasterized, uploaded and destroyed each frame.
  Destroying those GPU textures also breaks batching/flushes queued draws. A
  bounded 256-entry / 4 MiB CPU text cache (roughly matching GPU texel budget)
  now retains both immutable rasters and GPU textures. All text paths use it;
  unchanged labels have zero repeat rasterizations/uploads. Caller surface and
  texture ownership is preserved by references; LRU eviction is safe even with
  a retained surface/texture. Changed strings rasterize only on cache miss.
- Replaced up to 544 sky scanline/color commands each frame with one cached
  gradient-texture draw. Two 1x256 textures cover clear and overcast sky.
- Explicitly enable SDL render batching. Sprite keys/composites were already
  startup-only/cached; ordinary tiled frames use a single tile, while uncommon
  cross-tile scaled crops stay cached. There is no CPU full-frame compositor to
  dirty-redraw: native GXM redraws the GPU scene and culls offscreen floors.
- Simulation uses elapsed time, independent of render FPS: normal/paused
  maintenance 15 Hz, fast 60 Hz, turbo 180 Hz, with bounded catch-up to avoid a
  spiral after suspend/slow frames. Desktop/web pacing remains unchanged.
- Preserve startup clocks ARM 444 / bus 222 / GPU 222 / xbar 166 MHz, now logging
  actual readbacks as well as setter status.

Every ~5 seconds boot.log gets a PERF line: avg_fps, min_fps (slowest frame's
reciprocal), worst_frame_ms, simulation/input work, render CPU time, presentation
wait, actual texture uploads, tiled pieces, scaled-cache misses and text
hits/misses. The first window includes startup texture uploads/warmup; later
idle windows should show cache hits and few or zero uploads. Send at least two
PERF lines, plus whether touch and both stick roles now work. Logging itself is
infrequent but synced, so storage stalls may appear in worst-frame timing.

ASan/UBSan tests added: `test_vita_input.c` covers both stick roles, deadzone,
clamp, Cross drag, Circle, D-pad, Start, normalized front touch, rear rejection,
synthetic duplicate suppression, combined left-button ownership, focus release,
cursor drawing, and identical 15/60 Hz sim counts at 30/60/120 render fps.
`test_text_cache.c` uses a deterministic mock rasterizer to verify one raster
and upload across 100 unchanged draws, memory-budget eviction with free slots,
LRU eviction, retained-reference safety and cleanup. These are host tests, not
a claim of real-Vita 60 fps. Prior real-EXE sprite/tiling regressions still pass.
Release: `ConciliaTower-v0.7.vpk`, APP_VER 00.07; same indexed artwork, spaced
menu title, no proprietary EXE/resource files or shader compiler in package.


## v0.8: 1.5x world zoom, tool shortcuts and rooted save slots (APP_VER 00.08)

- World-only 3:2 nearest zoom: render into a reusable 640x364 GPU target,
  upscale to 960x546 and crop one output row at top/bottom. UI, Map and pointer
  stay at 960x544. Adjacent/tiled sprites are composed before the single upscale,
  avoiding per-sprite fractional phases/seams and per-frame CPU uploads.
- Picking, person hits, camera clamp, right-click camera drag and minimap
  viewport use the zoom transform. Tenant preview windows retain native scale.
  Target support is checked at startup; unsupported targets fall back to 1x
  coherently and log the reason, while later bind failures display an error.
- Buttons use the table above. Cross/R/touch form a single held left-button
  state, avoiding early mouse-up during combined input. Tool buttons use the
  same selection function as the Tools window so its highlight stays correct.
- Save/Save As/Load and both export formats use writable userdata only.
  Save As and Load show the folder path and existing .sav files. A safe basename
  policy prevents traversal/absolute overrides; directory creation and all
  native save/load paths/results are logged. Existing atomic temporary-write
  and three backup generations remain intact. Save slots are session-selected;
  startup still auto-loads the original default filename.
- Host sanitizer tests cover the controller changes, safe save paths/listing,
  actual save/load roundtrip (money, clock, tuning, backup generation), nearest
  world composition against a raw SDL reference with forced 127px tiles,
  negative/positive camera inverse transforms, and unchanged UI dimensions.
  The full simulation suite has four existing shaft-cap fixture failures,
  reproduced byte-for-byte with the unchanged v0.7 game.c; no new failures.
- Release filename: `ConciliaTower-v0.8.vpk`; PNGs remain 8-bit indexed. Hardware
  confirmation is still needed for zoom sampling/picking, new buttons and saves.


## v0.9: readable compact UI, contextual controls and native cursors (APP_VER 00.09)

Only Tools (1.25x, 32px item buttons become 40px) and Tower Info (1.1x,
including text/stars/date/funds) grow. Map, menu bar and other dialogs stay
native size. Tools starts beside Map, Info is right-aligned; drag bounds and
side flyouts stay on-screen. Both enlarged panels share a reusable GPU canvas,
with a single nearest-filtered upscale per panel and no per-frame CPU uploads.
The tuning list is paged on Vita so its final controls remain reachable.

Controls (supersede earlier release tables):
- Left stick camera; right stick visible pointer; L right click; front touch tap/drag.
- Outside menus/dialogs: Cross/R left click/drag; Circle Hand; Triangle Inspect;
  Square Bulldozer; Start opens/toggles the game menu.
- In menus/dialogs: D-pad moves a visible yellow/blue focus; Cross or Circle
  activates the focused/default control; Triangle cancels/closes without
  confirming (or choose a Cancel button). R/touch still click at the pointer.
- With a build item selected, D-pad moves exactly one horizontal cell or one
  floor per press, with hold-repeat and camera-edge scrolling. UI focus takes
  priority. Hand/Inspect/no-build mode keeps fine pointer nudges.

Runtime cursor extraction uses the original RT_GROUP_CURSOR/RT_CURSOR pairs:
1002 crosshair for building, 1003 magnifier, 1004 hand, 1005 demolition X.
Monochrome AND/XOR masks, bottom-up row strides and native hotspots are decoded;
inversion is used when the renderer supports it, otherwise the standard arrow
is the fallback. No original cursor assets are packaged. The fallback arrow now
has a correctly outlined diagonal stem instead of the old detached rectangle.
All cursors remain 1x and are drawn after the UI/focus highlight.

The linker reserves an explicit 16 KiB gap for VitaSDK import/module metadata;
plain 4 KiB segment alignment could make vita-elf-create fail as code grows.
Build/package validated; Tyler will confirm v0.9 on hardware. No release/push.

## v0.10: Vita Tower, Tool Sizing, in-game manual (APP_VER 00.10)

* **Rename:** LiveArea/SFO title, launch-gate art, icon and in-game strings now
  say **Vita Tower**. Title ID stays `CTWR00001`; saves stay in
  `ux0:data/concilia-tower/`, so existing towers keep working.
* **Tool Sizing menu** (top menu bar, before Help): *Original size*, *25% larger*
  (default), *50% larger*. Rescales the Tools window, buttons and tap areas at
  runtime; the current choice is checkmarked. It is saved as `tool_size=NNN` in
  `ux0:data/concilia-tower/prefs.cfg` and logged to `boot.log`. If 50% would
  push the Tools window past the bottom of the screen (many unlocked rows), the
  scale is reduced only as far as needed to fit. Tower Info is right-aligned and
  does not overlap; the Map is never resized.
* **Manual:** Help > *Vita Tower Manual* opens a scrollable in-game manual
  (6 sections: Controls, Building, Tools and Tower Info, Tool Sizing, Saving and
  loading, Playing Vita Tower). D-pad up/down or right stick scrolls; D-pad
  left/right (or L, previous) switches section; Circle/Triangle/Start close;
  Cross presses the focused button (Close by default); touch the buttons or drag
  the text. The same text ships as system e-manual pages in
  `sce_sys/manual/001-006.png` for the LiveArea Manual entry.
* Source: `vita/manual/manual.txt` -> `python3 tools/manual_gen.py` regenerates
  `src/manual_text.h` and the 8-bit indexed manual PNGs.

## v0.11: official art, right-side gate, black boot (APP_VER 00.11)

* **Official Vita Tower art** (Tyler), converted by `tools/brand_art.py`
  (source JPGs are not committed; only the 8-bit indexed, dithered PNGs):
  * `sce_sys/icon0.png` 128x128: round dome illustration (bubble mask hides corners)
  * `sce_sys/livearea/contents/startup.png` 280x158: VITA TOWER logo on sky;
    logo and tagline sit above the bottom ~40 px where the system draws Start
  * `sce_sys/livearea/contents/bg.png` 840x500: wide tower and helicopter key art
  * `sce_sys/pic0.png` 960x544: same key art, shown while the app loads
* **LiveArea style `a3`** (was `a1`): the Start gate sits on the right of the
  card, over the sky, leaving the tower on the left visible. Gate position is
  fixed by the style; verify on hardware.
* **Boot:** the post-EXE-check screen is black (was navy 14,22,48, which showed
  as a solid blue screen through loading) with small light-gray "Loading..."
  bottom-right, presented before sprite decode.
* Tools and Tower Info keep their v0.10 default positions.
