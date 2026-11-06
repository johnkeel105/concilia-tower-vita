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
