# ConciliaTower on PS Vita (VitaGL)

This tree can be built as a PS Vita `.vpk` using VitaSDK and **sdl2_vitagl**
(SDL2 with the vitaGL video backend).

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

Saves go to `ux0:data/concilia-tower/concilliatower.sav`.

Native `.sav` files are architecture-specific (the Vita build is 32-bit, so
they are not interchangeable with desktop 64-bit saves). Use original
`.TDT`/`.TWR` files to move towers between the Vita port, the desktop/web
builds, and SimTower 1.1.

## Controls

| Vita | Action |
| --- | --- |
| Front touch | Mouse (click to build / UI) |
| Left analog | Pan camera |
| Right analog | Pan camera (faster) |
| D-pad | Pan camera |
| Cross | Confirm / Enter |
| Circle | Cancel / Esc |
| Start | Pause (Space) |
| Select | Debug labels |
| L / R | Simulation speed down / up |
| Triangle | Quick-save (F5) |

## Runtime notes (vitaGL shaders)

sdl2_vitagl / vitaShaRK compile shaders at runtime. On a real Vita or an
emulator this typically needs `libshacccg.suprx` installed in the console's
system data (commonly `ur0:data/libshacccg.suprx` or
`ur0:data/external/libshacccg.suprx`), plus taiHEN on hardware.

That module is **not** part of this project and must never be committed or
packed into the VPK. Install it separately if your device/emulator does not
already have it.

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
