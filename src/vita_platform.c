/* Vita / vitaGL helpers for the ConciliaTower port. */
#include "vita_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <vitaGL.h>
#include "debugScreen.h"
#include "exe_prepare.h"

#include <SDL_ttf.h>
#include <psp2/io/stat.h>
#include <psp2/power.h>

#define CT_DATA_DIR "ux0:data/concilia-tower"

extern int ct_kwaj_expand(const char *in_path, const char *out_path);

/* Reserve CPU heap explicitly; leave RAM/CDRAM for vitaGL's separate pools.
 * The SDK default was 128 MiB, not the desktop's virtually unlimited heap. */
unsigned int _newlib_heap_size_user = 192 * 1024 * 1024;
static char search_report[4096];
static int log_writable;
static int debug_screen_active;

void vita_boot_log(const char *fmt, ...)
{
    /* A closed file after EVERY record survives later aborts/graphics faults.
     * Never use SDL logging here (it can recurse through our callback). */
    FILE *f = fopen(CT_DATA_DIR "/boot.log", "a");
    if (!f) { log_writable = 0; return; }
    log_writable = 1;
    fprintf(f, "[%llu us] ", (unsigned long long)sceKernelGetProcessTimeWide());
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    int wrote = fputc('\n', f) != EOF;
    int flushed = fflush(f) == 0;
    int closed = fclose(f) == 0;
    log_writable = wrote && flushed && closed;
    sceIoSync("ux0:", 0);
}

static void sdl_log_output(void *unused, int category, SDL_LogPriority priority,
                           const char *message)
{
    vita_boot_log("SDL[%d/%d]: %s", category, priority, message);
}

void vita_boot_begin(void)
{
    int data_rc = sceIoMkdir("ux0:data", 0777);
    int dir_rc = sceIoMkdir(CT_DATA_DIR, 0777);
    /* Keep last run available even if the new run fails very early. */
    sceIoRemove(CT_DATA_DIR "/boot.previous.log");
    sceIoRename(CT_DATA_DIR "/boot.log", CT_DATA_DIR "/boot.previous.log");
    vita_boot_log("ConciliaTower v0.3 APP_VER=00.03 build=%s: app start", CT_BUILD_VERSION);
    vita_boot_log("kernel process clock available; mkdir ux0:data=0x%08x data dir=0x%08x; log writable=%d",
                  data_rc, dir_rc, log_writable);
    vita_boot_log("newlib heap=%u MiB; startup diagnostics use no SDL/GL", _newlib_heap_size_user >> 20);
    if (!log_writable) vita_boot_fatal("Cannot write boot.log. Check ux0:data/concilia-tower/ free space/write access.");
    SDL_LogSetOutputFunction(sdl_log_output, NULL);
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    vita_boot_log("shader-library preflight follows (presence is not proof the module can load)");
    vita_shader_library_present();
}

void vita_boost_clocks(void)
{
    vita_boot_log("clock setup begin");
    int arm = scePowerSetArmClockFrequency(444);
    int bus = scePowerSetBusClockFrequency(222);
    int gpu = scePowerSetGpuClockFrequency(222);
    int xbar = scePowerSetGpuXbarClockFrequency(166);
    vita_boot_log("clock setup done: arm=%d bus=%d gpu=%d xbar=%d", arm, bus, gpu, xbar);
}

void vita_prepare_userdata(void)
{
    int rc = sceIoMkdir(CT_DATA_DIR, 0777);
    vita_boot_log("data dir recheck: 0x%08x (already exists is normal)", rc);
}

const char *vita_save_path(void)
{
    return CT_DATA_DIR "/concilliatower.sav";
}

int vita_probe_file(const char *path)
{
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    int rc = sceIoGetstat(path, &st);
    char line[256];
    snprintf(line, sizeof(line), "%s: %s size=%llu (stat=0x%08x)", path,
             rc >= 0 ? "FOUND" : "missing", (unsigned long long)st.st_size, rc);
    vita_boot_log("%s", line);
    if (strstr(path, "SIMTOWER") || strstr(path, "simtower") || strstr(path, "SimTower")) {
        size_t n = strlen(search_report);
        snprintf(search_report + n, sizeof(search_report) - n, "%s\n", line);
    }
    if (rc < 0 || st.st_size == 0) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) { vita_boot_log("file unreadable: %s errno=%d", path, errno); return 0; }
    fclose(f);
    return 1;
}

int vita_shader_library_present(void)
{
    int a = vita_probe_file("ur0:data/libshacccg.suprx");
    int b = vita_probe_file("ur0:data/external/libshacccg.suprx");
    return a || b;
}

void vita_try_expand_ex_(void)
{
    const char *dest = CT_DATA_DIR "/SIMTOWER.EXE";
    static const char *plain[] = {
        CT_DATA_DIR "/SIMTOWER.EXE", CT_DATA_DIR "/simtower.exe",
        CT_DATA_DIR "/SimTower.exe", "ux0:data/SIMTOWER.EXE",
        "ux0:data/simtower.exe", "ux0:data/SimTower.exe", NULL
    };
    static const char *cands[] = {
        CT_DATA_DIR "/SIMTOWER.EX_", CT_DATA_DIR "/simtower.ex_",
        CT_DATA_DIR "/SimTower.ex_", "ux0:data/SIMTOWER.EX_",
        "ux0:data/simtower.ex_", "ux0:data/SimTower.ex_", NULL
    };
    int have = 0;
    const char *compressed = NULL;
    for (int i = 0; plain[i]; i++) have |= vita_probe_file(plain[i]);
    for (int i = 0; cands[i]; i++)
        if (vita_probe_file(cands[i]) && !compressed) compressed = cands[i];
    if (have || !compressed) return;
    /* Same validation/temporary-file/backup path as a misnamed KWAJ .EXE. */
    int rc = ct_prepare_exe(compressed, dest, vita_boot_notice);
    vita_boot_log("compressed EX_ preparation result=%d", rc);
    if (rc < 0) vita_boot_fatal("SIMTOWER.EX_ expansion failed; original preserved. See boot.log.");
    sceIoSync("ux0:", 0);
}

void vita_init_runtime(void)
{
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameControllerOpen(i);
            break;
        }
    }
}

int vita_remap_event(SDL_Event *ev)
{
    if (ev->type != SDL_CONTROLLERBUTTONDOWN &&
        ev->type != SDL_CONTROLLERBUTTONUP)
        return 0;

    int down = (ev->type == SDL_CONTROLLERBUTTONDOWN);
    SDL_Keycode k = SDLK_UNKNOWN;
    switch (ev->cbutton.button) {
    case SDL_CONTROLLER_BUTTON_A:            k = SDLK_RETURN;     break; /* Cross */
    case SDL_CONTROLLER_BUTTON_B:            k = SDLK_ESCAPE;     break; /* Circle */
    case SDL_CONTROLLER_BUTTON_START:        k = SDLK_SPACE;      break;
    case SDL_CONTROLLER_BUTTON_BACK:         k = SDLK_BACKQUOTE;  break; /* Select */
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:    k = SDLK_LEFT;       break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:   k = SDLK_RIGHT;      break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:      k = SDLK_UP;         break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:    k = SDLK_DOWN;       break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: k = SDLK_MINUS;      break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:k = SDLK_PLUS;       break;
    case SDL_CONTROLLER_BUTTON_Y:            k = SDLK_F5;         break; /* Triangle: save */
    default:
        return 1;
    }

    ev->type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev->key.keysym.sym = k;
    ev->key.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev->key.repeat = 0;
    ev->key.keysym.mod = KMOD_NONE;
    ev->key.keysym.scancode = SDL_GetScancodeFromKey(k);
    return 0;
}

void vita_pan_camera(float *cam_fx, float *cam_fy)
{
    static SDL_GameController *gc;
    if (!gc) {
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                gc = SDL_GameControllerOpen(i);
                break;
            }
        }
        if (!gc)
            return;
    }

    const int dead = 8000;
    int ax = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
    int ay = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
    int rx = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX);
    int ry = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY);
    if (abs(ax) > dead) *cam_fx += (ax / 32768.0f) * 14.0f;
    if (abs(ay) > dead) *cam_fy += (ay / 32768.0f) * 14.0f;
    if (abs(rx) > dead) *cam_fx += (rx / 32768.0f) * 26.0f;
    if (abs(ry) > dead) *cam_fy += (ry / 32768.0f) * 26.0f;
}

/* Fatal boot notice: direct display framebuffer + SDK bitmap font. No
 * SDL window, TTF, vitaGL, shader compiler, or EXE resources are required.
 * Do not SDL_Quit here: cleanup after half-initialized GL can itself crash. */
void vita_boot_notice(const char *message)
{
    vita_boot_log("%s", message);
    if (!debug_screen_active) debug_screen_active = psvDebugScreenInit() >= 0;
    if (debug_screen_active)
        psvDebugScreenPrintf("\e[2J\e[1;1HConciliaTower v0.3\n\n%s\n\nLog: ux0:data/concilia-tower/boot.log\n", message);
}

void vita_boot_notice_end(void)
{
    if (debug_screen_active) psvDebugScreenFinish();
    debug_screen_active = 0;
}

int vita_boot_fatal(const char *reason)
{
    char error[512];
    snprintf(error, sizeof(error), "%s", SDL_GetError());
    vita_boot_log("FATAL: %s; SDL_GetError=%s", reason, error);
    int rc = debug_screen_active ? 0 : psvDebugScreenInit();
    vita_boot_log("direct framebuffer error screen init=0x%08x", rc);
    if (rc >= 0) {
        psvDebugScreenPrintf("\e[2J\e[1;1HConciliaTower v0.3 - startup stopped\n\n%s\nSDL: %s\n\n", reason, error);
        psvDebugScreenPrintf("Log: ux0:data/concilia-tower/boot.log\nLog writable: %s\n\n", log_writable ? "yes" : "NO - check free space/write access");
        psvDebugScreenPrintf("EXE search (FOUND = detected, not yet validated):\n%s\n", search_report);
        psvDebugScreenPrintf("\nSend boot.log and a photo of this screen.\nRelease buttons, then press START or Circle to exit.\n");
    }
    SceCtrlData pad;
    int released = 0;
    for (;;) {
        memset(&pad, 0, sizeof(pad));
        if (sceCtrlPeekBufferPositive(0, &pad, 1) > 0) {
            unsigned mask = SCE_CTRL_START | SCE_CTRL_CIRCLE;
            if (!(pad.buttons & mask)) released = 1;
            if (released && (pad.buttons & mask)) break;
        }
        sceKernelDelayThread(20000);
    }
    vita_boot_log("user acknowledged startup failure; exit requested");
    sceKernelExitProcess(1);
    return 1;
}

int vita_missing_exe_screen(void)
{
    return vita_boot_fatal("SIMTOWER.EXE missing/unreadable. Copy it (or SIMTOWER.EX_) to ux0:data/concilia-tower/.");
}

static void log_free_memory(const char *stage)
{
    SceKernelFreeMemorySizeInfo mem = {0};
    mem.size = sizeof(mem);
    int rc = sceKernelGetFreeMemorySize(&mem);
    vita_boot_log("memory %s: query=0x%08x user=%d CDRAM=%d phycont=%d bytes",
                  stage, rc, mem.size_user, mem.size_cdram, mem.size_phycont);
}

/* SDL's installed vitaGL backend ignores vglInitExtended's return value.
 * Interpose the call so a real GL_FALSE does not continue into invalid GL.
 * GL_TRUE is SUCCESS, unlike SDL's zero-success convention. */
GLboolean __real_vglInitExtended(int, int, int, int, SceGxmMultisampleMode);
GLboolean __wrap_vglInitExtended(int pool, int width, int height, int threshold,
                                SceGxmMultisampleMode msaa)
{
    log_free_memory("before vitaGL");
    vita_boot_log("vitaGL init begin: legacy=%d size=%dx%d RAM threshold=%d MSAA=%d", pool, width, height, threshold, msaa);
    GLboolean ok = __real_vglInitExtended(pool, width, height, threshold, msaa);
    vita_boot_log("vitaGL init returned %d (GL_TRUE=%d)", ok, GL_TRUE);
    log_free_memory("after vitaGL");
    if (ok == GL_FALSE) vita_boot_fatal("vitaGL initialization failed. Check memory and boot.log.");
    return ok;
}

/* SDL resolves GL entry points via vglGetProcAddress, not direct linkage. */
static void diagnostic_compile_shader(GLuint shader)
{
    vita_boot_log("shader compile begin id=%u", shader);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    char info[2048] = {0};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    glGetShaderInfoLog(shader, sizeof(info), NULL, info);
    vita_boot_log("shader compile done id=%u status=%d info=%s", shader, ok, info);
    if (!ok) vita_boot_fatal("Shader compilation failed. See boot.log compiler output.");
}

static void diagnostic_link_program(GLuint program)
{
    vita_boot_log("shader program link begin id=%u", program);
    glLinkProgram(program);
    GLint ok = GL_FALSE;
    char info[2048] = {0};
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glGetProgramInfoLog(program, sizeof(info), NULL, info);
    vita_boot_log("shader program link done id=%u status=%d info=%s", program, ok, info);
    if (!ok) vita_boot_fatal("Shader program link failed. See boot.log.");
}

void *__real_vglGetProcAddress(const char *name);
void *__wrap_vglGetProcAddress(const char *name)
{
    if (!strcmp(name, "glCompileShader")) return (void *)diagnostic_compile_shader;
    if (!strcmp(name, "glLinkProgram")) return (void *)diagnostic_link_program;
    return __real_vglGetProcAddress(name);
}
