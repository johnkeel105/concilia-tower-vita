cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(concilia-tower C CXX)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(Freetype REQUIRED)

set(VITA_APP_NAME "Concilia Tower")
set(VITA_TITLEID  "CTWR00001")
set(VITA_VERSION  "00.09")

execute_process(
  COMMAND git describe --always --dirty
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE CT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT CT_VERSION)
  set(CT_VERSION "unknown")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -O2 -Wall -Wno-unused-parameter -fstack-usage")

add_executable(${PROJECT_NAME}
  src/main.c
  src/exe_prepare.c
  src/ne_resource.c
  src/sprites.c
  src/texture.c
  src/text_cache.c
  src/vita_input.c
  src/world_view.c
  src/ui_scale.c
  src/ui_focus.c
  src/cursor.c
  src/save_store.c
  src/tower.c
  src/game.c
  src/people.c
  src/twr.c
  src/audio.c
  src/sound_hook.c
  src/strings.c
  src/kwaj_expand.c
  src/vendor/mspack/kwajd.c
  src/vendor/mspack/mszipd.c
  src/vendor/mspack/lzssd.c
  src/vendor/mspack/system.c
  "$ENV{VITASDK}/share/gcc-arm-vita-eabi/samples/common/debugScreen.c"
  src/vita_platform.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
  src
  src/vendor/mspack
  "$ENV{VITASDK}/share/gcc-arm-vita-eabi/samples/common"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  CT_BUILD_VERSION="${CT_VERSION}"
)

target_link_options(${PROJECT_NAME} PRIVATE
  -Wl,--wrap=vglInitExtended -Wl,--wrap=vglGetProcAddress
  "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/vita/metadata-gap.ld")

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)


if(TARGET SDL2_ttf::SDL2_ttf)
  set(CT_SDL2_TTF SDL2_ttf::SDL2_ttf)
else()
  set(CT_SDL2_TTF SDL2_ttf::SDL2_ttf-static)
endif()

# sdl2_vitagl already INTERFACE-links vitaGL, vitashark, mathneon and the
# common stubs. SceShaccCgExt + taihen are required by vitaShaRK at link
# time; the shader compiler module itself (libshacccg.suprx) is a runtime
# extra on the device/emulator and must NOT be packaged in the VPK.
target_link_libraries(${PROJECT_NAME}
  ${CT_SDL2_TTF}
  SDL2::SDL2main
  SDL2::SDL2
  Freetype::Freetype
  vitaGL
  vitashark
  SceShaccCgExt
  taihen_stub
  mathneon
  SceShaccCg_stub
  SceKernelDmacMgr_stub
  SceIme_stub
  SceGxm_stub
  SceDisplay_stub
  SceCtrl_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceAudio_stub
  SceAudioIn_stub
  SceSysmodule_stub
  SceIofilemgr_stub
  SceCommonDialog_stub
  SceTouch_stub
  SceHid_stub
  SceMotion_stub
  ScePower_stub
  SceProcessmgr_stub
  png
  z
  bz2
  m
  stdc++
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE STRIPPED)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE vita/sce_sys/icon0.png sce_sys/icon0.png
  FILE vita/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE vita/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE vita/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  FILE web/fonts/LiberationSans-Bold.ttf fonts/LiberationSans-Bold.ttf
  FILE web/fonts/LiberationSans-Regular.ttf fonts/LiberationSans-Regular.ttf
)
