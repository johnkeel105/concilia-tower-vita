/* Vita / vitaGL helpers for the ConciliaTower port. */
#include "vita_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <stdatomic.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <vitaGL.h>
#include "debugScreen.h"
#include "exe_prepare.h"
#include "text_cache.h"
#include "save_store.h"
#include <psp2/touch.h>

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
static SceDisplayFrameBuf error_frame;
static int error_frame_reserved;
static _Atomic(const char *) watched_stage;
static atomic_uint watched_since;
static atomic_uint watched_generation;
static atomic_int watchdog_finished;

/* Stages are static string literals. The watchdog never touches SDL/GL or
 * cancels the render thread: a timeout is evidence, not a safe recovery point. */
void vita_boot_watch_stage(const char *stage)
{
    if (stage && watchdog_finished) return;
    if (!stage) watchdog_finished = 1;
    watched_since = sceKernelGetProcessTimeLow();
    watched_stage = stage;
    atomic_fetch_add(&watched_generation, 1);
}

static int boot_watchdog(SceSize args, void *argp)
{
    unsigned generation = 0;
    SceUInt32 last_report = 0;
    for (;;) {
        sceKernelDelayThread(1000000);
        if (watchdog_finished) return sceKernelExitDeleteThread(0);
        unsigned current = watched_generation;
        const char *stage = watched_stage;
        SceUInt32 now = sceKernelGetProcessTimeLow();
        SceUInt32 elapsed = now - watched_since;
        if (current != watched_generation) continue;
        if (current != generation) { generation = current; last_report = now; }
        if (stage && elapsed >= 60000000U && now - last_report >= 30000000U) {
            vita_boot_log("WATCHDOG TIMEOUT: no stage completion for %u seconds: %s (diagnostic only; no forced reset)", elapsed / 1000000U, stage);
            last_report = now;
        }
    }
    return 0;
}

/* Allocate the shader-independent error display BEFORE vitaGL takes pools.
 * Keep it reserved, hidden behind SDL's framebuffer during normal rendering. */
static void reserve_error_frame(void)
{
    if (error_frame_reserved) return;
    int rc = debug_screen_active ? 0 : psvDebugScreenInit();
    debug_screen_active = rc >= 0;
    memset(&error_frame, 0, sizeof(error_frame));
    error_frame.size = sizeof(error_frame);
    if (rc >= 0) {
        rc = sceDisplayGetFrameBuf(&error_frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
        error_frame_reserved = rc >= 0 && error_frame.base != NULL;
    }
    vita_boot_log("reserved 2 MiB direct error framebuffer before GL: result=0x%08x ready=%d", rc, error_frame_reserved);
}

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
    vita_boot_log("Concilia Tower v0.8 APP_VER=00.08 build=%s: app start", CT_BUILD_VERSION);
    vita_boot_log("kernel process clock available; mkdir ux0:data=0x%08x data dir=0x%08x; log writable=%d",
                  data_rc, dir_rc, log_writable);
    vita_boot_log("newlib heap=%u MiB; startup diagnostics use no SDL/GL", _newlib_heap_size_user >> 20);
    if (!log_writable) vita_boot_fatal("Cannot write boot.log. Check ux0:data/concilia-tower/ free space/write access.");
    SDL_LogSetOutputFunction(sdl_log_output, NULL);
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    vita_boot_log("shader-library presence check is informational only; native GXM does not load the runtime compiler");
    vita_shader_library_present();
    extern const char *GLES2_GetShader(int type);
    const char *historic_shader = GLES2_GetShader(6);
    vita_boot_log("v0.4 failed shader #7 = SDL GLES2 table type 6 GLES2_SHADER_FRAGMENT_TEXTURE_YUV_JPEG (Cg); unused by this RGB-only game");
    vita_boot_log("installed historical shader #7 source BEGIN\n%s\nsource END", historic_shader ? historic_shader : "(not present)");
    reserve_error_frame();
    SceUID watchdog = sceKernelCreateThread("ct_boot_watchdog", boot_watchdog,
                                           0x10000100, 64 * 1024, 0, 0, NULL);
    int rc = watchdog < 0 ? watchdog : sceKernelStartThread(watchdog, 0, NULL);
    vita_boot_log("boot watchdog start=0x%08x (60s stage timeout, then every 30s)", rc);
    vita_boot_watch_stage("EXE detection / expansion / resource parse");
}

void vita_boost_clocks(void)
{
    vita_boot_log("clock setup begin");
    int arm = scePowerSetArmClockFrequency(444);
    int bus = scePowerSetBusClockFrequency(222);
    int gpu = scePowerSetGpuClockFrequency(222);
    int xbar = scePowerSetGpuXbarClockFrequency(166);
    vita_boot_log("clock setup status: arm=%d bus=%d gpu=%d xbar=%d; actual MHz ARM=%d bus=%d GPU=%d xbar=%d", arm, bus, gpu, xbar, scePowerGetArmClockFrequency(),scePowerGetBusClockFrequency(),scePowerGetGpuClockFrequency(),scePowerGetGpuXbarClockFrequency());
}

void vita_prepare_userdata(void)
{
    int rc = sceIoMkdir(CT_DATA_DIR, 0777);
    vita_boot_log("data dir recheck: 0x%08x (already exists is normal); save directory=%s ready=%d", rc,CT_VITA_SAVE_DIR,CT_SaveDirectoryCreate(CT_VITA_SAVE_DIR));
}

static char current_save[512]=CT_VITA_SAVE_DIR "/concilliatower.sav";
const char *vita_save_path(void) { return current_save; }
int vita_set_save_name(const char *name)
{
    char next[512];if(CT_SavePathBuild(CT_VITA_SAVE_DIR,name,next,sizeof(next))<0) return -1;
    strcpy(current_save,next);vita_boot_log("active save path=%s",current_save);return 0;
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

static SDL_GameController *controller;
static CT_VitaInput pointer;
static int sdl_front_seen, sdk_touch_mode;
static SceTouchPanelInfo front_panel;
static double present_ms;

void vita_input_hints(void)
{
    /* Translate front FINGER events ourselves: one click stream, no duplicated
     * SDL_TOUCH_MOUSEID mouse or mouse-to-touch feedback. Installed SDL uses
     * these environment gates and registers front ID=1 / back ID=2. */
    unsetenv("VITA_DISABLE_TOUCH_FRONT");
    SDL_setenv("VITA_DISABLE_TOUCH_BACK","1",1);
    SDL_SetHintWithPriority(SDL_HINT_TOUCH_MOUSE_EVENTS,"0",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_MOUSE_TOUCH_EVENTS,"0",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_VITA_TOUCH_MOUSE_DEVICE,"0",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_BATCHING,"1",SDL_HINT_OVERRIDE);
}
void vita_init_runtime(void)
{
    for(int i=0;i<SDL_NumJoysticks();i++) if(SDL_IsGameController(i)) {
        controller=SDL_GameControllerOpen(i);if(controller) break;
    }
    SDL_TouchID front=1;
    for(int i=0;i<SDL_GetNumTouchDevices();i++) {
        SDL_TouchID id=SDL_GetTouchDevice(i);const char *name=SDL_GetTouchName(i);
        vita_boot_log("touch device index=%d id=%lld name=%s",i,(long long)id,name?name:"(null)");
        if(name && !SDL_strcasecmp(name,"Front")) front=id;
    }
    CT_InputInit(&pointer,960,544,front);
    int rc=sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START);
    memset(&front_panel,0,sizeof(front_panel));
    int panel_rc=sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT,&front_panel);
    vita_boot_log("input v0.8: LEFT=camera RIGHT=cursor D-pad=fine pointer Cross/R=left-drag L=right Circle=Hand Triangle=Inspect Square=Bulldozer Start=menu; controller=%p",(void*)controller);
    vita_boot_log("front touch manual normalized->960x544; SDL synthetic mouse disabled; sampling=0x%x panel=0x%x area=%d,%d..%d,%d",rc,panel_rc,front_panel.minAaX,front_panel.minAaY,front_panel.maxAaX,front_panel.maxAaY);
}
void vita_input_event(const SDL_Event *ev,CT_InputSink sink)
{
    if(ev->type==SDL_FINGERDOWN || ev->type==SDL_FINGERMOTION || ev->type==SDL_FINGERUP) {
        if(sdk_touch_mode) return;
        if(ev->tfinger.touchId==pointer.front && !sdl_front_seen) {
            sdl_front_seen=1;vita_boot_log("front SDL finger input received: normalized x=%.3f y=%.3f",ev->tfinger.x,ev->tfinger.y);
        }
    }
    if(!CT_InputEvent(&pointer,ev,sink)) {
        SDL_Event copy=*ev;
        if(copy.type==SDL_MOUSEMOTION) { copy.motion.x=(int)pointer.x;copy.motion.y=(int)pointer.y; }
        else if(copy.type==SDL_MOUSEBUTTONDOWN || copy.type==SDL_MOUSEBUTTONUP) {
            copy.button.x=(int)pointer.x;copy.button.y=(int)pointer.y;
        }
        sink(&copy);
    }
}
int vita_update_input(double dt,float *cx,float *cy,CT_InputSink sink)
{
    /* Some SDL/Vita backend variants gate touch on their private window.
     * If the first real front contact produces no SDL FINGER event, permanently
     * select SDK front polling for this run; never emit two click streams. */
    if(!sdl_front_seen || sdk_touch_mode) {
        SceTouchData data;
        int peek=sceTouchPeek(SCE_TOUCH_PORT_FRONT,&data,1);
        static int peek_warned;
        if(peek<0 && !peek_warned) { vita_boot_log("front SDK touch peek failed: 0x%x",peek);peek_warned=1; }
        if(peek>0) {
            if(data.reportNum && !sdk_touch_mode) {
                sdk_touch_mode=1;vita_boot_log("front contact seen by SDK but no SDL finger event: SDK touch fallback selected");
            }
            if(sdk_touch_mode) {
                int n=-1;
                for(unsigned i=0;i<data.reportNum;i++)
                    if(!pointer.touch || data.report[i].id==(int)pointer.finger) { n=(int)i;break; }
                if(n>=0) {
                    float w=front_panel.maxAaX-front_panel.minAaX,h=front_panel.maxAaY-front_panel.minAaY;
                    float x=(data.report[n].x-front_panel.minAaX)/(w>0?w:1919);
                    float y=(data.report[n].y-front_panel.minAaY)/(h>0?h:1087);
                    CT_InputTouch(&pointer,pointer.touch?SDL_FINGERMOTION:SDL_FINGERDOWN,x,y,data.report[n].id,sink);
                } else if(pointer.touch) CT_InputTouch(&pointer,SDL_FINGERUP,pointer.x/960,pointer.y/544,pointer.finger,sink);
            }
        }
    }
    if(controller && !SDL_GameControllerGetAttached(controller)) { SDL_GameControllerClose(controller);controller=NULL; }
    int lx=0,ly=0,rx=0,ry=0;
    if(controller) {
        lx=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX);
        ly=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY);
        rx=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_RIGHTX);
        ry=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_RIGHTY);
    }
    return CT_InputStep(&pointer,(float)dt,lx,ly,rx,ry,cx,cy,sink);
}
void vita_draw_cursor(SDL_Renderer *r) { CT_InputDraw(&pointer,r); }
void vita_present(SDL_Renderer *r)
{
    Uint64 a=SDL_GetPerformanceCounter();SDL_RenderPresent(r);
    present_ms=1000.0*(SDL_GetPerformanceCounter()-a)/SDL_GetPerformanceFrequency();
}
void vita_frame_finish(Uint64 start,double dt,double sim_ms,double render_ms)
{
    static Uint64 since;static unsigned frames;
    static double worst,sim_sum,render_sum,present_sum;
    Uint64 freq=SDL_GetPerformanceFrequency(),now=SDL_GetPerformanceCounter();
    double elapsed=(double)(now-start)/freq;
    /* Present already blocks on vsync. Only sleep any remaining frame budget,
     * never add the old unconditional 16 ms after waiting for vblank. */
    if(elapsed<1.0/60) {
        double remaining=(1.0/60-elapsed)*1000;
        if(remaining>=1) SDL_Delay((Uint32)remaining);
    }
    now=SDL_GetPerformanceCounter();if(!since) since=now;
    frames++;if(dt>worst) worst=dt;
    sim_sum+=sim_ms;render_sum+=SDL_max(0,render_ms-present_ms);present_sum+=present_ms;
    double seconds=(double)(now-since)/freq;
    if(seconds>=5) {
        CT_TextureStats tx=CT_TextureStatsReset();CT_TextStats text=CT_TextStatsReset();
        vita_boot_log("PERF %.1fs avg_fps=%.2f min_fps=%.2f worst_frame_ms=%.2f sim_ms=%.2f render_cpu_ms=%.2f present_ms=%.2f uploads=%u tile_pieces=%u scaled_cache_misses=%u text_hits=%u text_misses=%u",seconds,frames/seconds,worst>0?1/worst:0,worst*1000,sim_sum/frames,render_sum/frames,present_sum/frames,tx.uploads,tx.tile_draws,tx.scaled_cache_misses,text.hits,text.misses);
        since=now;frames=0;worst=sim_sum=render_sum=present_sum=0;
    }
}

/* Fatal boot notice: direct display framebuffer + SDK bitmap font. No
 * SDL window, TTF, vitaGL, shader compiler, or EXE resources are required.
 * Do not SDL_Quit here: cleanup after half-initialized GL can itself crash. */
void vita_boot_notice(const char *message)
{
    vita_boot_log("%s", message);
    reserve_error_frame();
    if (error_frame_reserved) sceDisplaySetFrameBuf(&error_frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
    if (debug_screen_active)
        psvDebugScreenPrintf("\e[2J\e[1;1HConcilia Tower v0.8\n\n%s\n\nLog: ux0:data/concilia-tower/boot.log\n", message);
}

void vita_boot_notice_end(void)
{
    /* Do NOT free: fatal notices must work after vitaGL owns the other pools.
     * SDL's first frame will replace this framebuffer. No GL initialization here. */
    vita_boot_log("progress screen hidden for SDL; error framebuffer remains reserved");
}

int vita_boot_fatal(const char *reason)
{
    char error[512];
    snprintf(error, sizeof(error), "%s", SDL_GetError());
    vita_boot_watch_stage(NULL);
    vita_boot_log("FATAL: %s; SDL diagnostic snapshot (may be stale, NOT root cause)=%s", reason, error);
    if (!error_frame_reserved) reserve_error_frame();
    int rc = error_frame_reserved ? sceDisplaySetFrameBuf(&error_frame, SCE_DISPLAY_SETBUF_NEXTFRAME) : -1;
    vita_boot_log("reserved direct framebuffer error screen restore=0x%08x", rc);
    if (rc >= 0) {
        psvDebugScreenPrintf("\e[2J\e[1;1HConcilia Tower v0.8 - startup stopped\n\n%s\nSDL diagnostic (may be stale): %s\n\n", reason, error);
        psvDebugScreenPrintf("Log: ux0:data/concilia-tower/boot.log\nLog writable: %s\n\n", log_writable ? "yes" : "NO - check free space/write access");
        psvDebugScreenPrintf("EXE search (FOUND = detected, not yet validated):\n%s\n", search_report);
        psvDebugScreenPrintf("\nSend boot.log and a photo of this screen.\nRelease buttons, then press START or Circle to exit.\n");
    }
    SceCtrlData pad;
    int released = 0;
    SceUInt32 wait_start = sceKernelGetProcessTimeLow();
    for (;;) {
        /* Retake display after any already-queued GL frame retires. This needs
         * neither a new allocation nor GL/shader calls, even on shader failure. */
        if (error_frame_reserved) sceDisplaySetFrameBuf(&error_frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
        if (rc < 0 && sceKernelGetProcessTimeLow() - wait_start >= 30000000U) {
            vita_boot_log("error display unavailable; leaving invisible failure screen after 30s");
            break;
        }
        memset(&pad, 0, sizeof(pad));
        if (sceCtrlPeekBufferPositive(0, &pad, 1) > 0) {
            unsigned mask = SCE_CTRL_START | SCE_CTRL_CIRCLE;
            if (!(pad.buttons & mask)) released = 1;
            if (released && (pad.buttons & mask)) break;
        }
        sceKernelDelayThread(20000);
    }
    vita_boot_log("startup failure exit requested (button or unavailable-display timeout)");
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

/* Exact installed vitaGL revision: 2bdbe89. Despite the GLboolean signature,
 * vglInitWithCustomSizes returns res_fallback, NOT an initialization status:
 * GL_FALSE = requested resolution accepted; GL_TRUE = resolution was clamped.
 * It also returns GL_FALSE for a suppressed duplicate. Never use it as success.
 * SDL is the sole initialization owner; this wrapper only observes/configures
 * its one call. Context/renderer state is validated after SDL creates them. */
GLboolean __real_vglInitExtended(int, int, int, int, SceGxmMultisampleMode);
static unsigned gl_init_calls;
static GLboolean gl_resolution_fallback;
GLboolean __wrap_vglInitExtended(int pool, int width, int height, int threshold,
                                SceGxmMultisampleMode msaa)
{
    if (gl_init_calls) {
        vita_boot_log("duplicate SDL vitaGL init request suppressed by port; no second initialization");
        return gl_resolution_fallback;
    }
    gl_init_calls = 1;
    vita_boot_watch_stage("SDL-owned vitaGL initialization");
    log_free_memory("before vitaGL");
    /* Threshold is RAM LEFT FREE, not pool size. Preserve 24 MiB of system RAM.
     * The preallocated error framebuffer already protects 2 MiB of CDRAM. */
    if (threshold < 24 * 1024 * 1024) threshold = 24 * 1024 * 1024;
    vglSetCircularPoolSize(4 * 1024 * 1024); /* 2D batch stream, formerly 32 MiB */
    vita_boot_log("single SDL-owned vitaGL init begin: legacy=%d size=%dx%d free RAM reserve=%d MSAA=%d circular=4 MiB", pool, width, height, threshold, msaa);
    gl_resolution_fallback = __real_vglInitExtended(pool, width, height, threshold, msaa);
    vita_boot_log("vitaGL returned resolution_fallback=%d (0=normal resolution; NOT a failure status)", gl_resolution_fallback);
    log_free_memory("after vitaGL");
    vita_boot_log("vitaGL pool state: total=%u free=%u bytes; awaiting SDL context/renderer validation",
                  (unsigned)vglMemTotal(VGL_MEM_ALL), (unsigned)vglMemFree(VGL_MEM_ALL));
    vita_boot_watch_stage("SDL_CreateWindow completion / context creation");
    return gl_resolution_fallback;
}

SDL_Renderer *vita_create_native_renderer(SDL_Window *window)
{
    int driver = -1;
    for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
        SDL_RendererInfo info;
        if (SDL_GetRenderDriverInfo(i, &info) == 0 && !strcmp(info.name, "VITA gxm")) {
            driver = i;
            break;
        }
    }
    if (driver < 0) { SDL_SetError("Installed SDL lacks native VITA gxm renderer"); return NULL; }
    if (gl_init_calls || (SDL_GetWindowFlags(window) & SDL_WINDOW_OPENGL)) {
        SDL_SetError("Refusing to mix native GXM and already initialized vitaGL");
        return NULL;
    }
    log_free_memory("before native GXM");
    SDL_ClearError();
    SDL_Renderer *renderer = SDL_CreateRenderer(window, driver,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE | SDL_RENDERER_PRESENTVSYNC);
    vita_boot_log("native renderer result=%p driver=%d name=VITA gxm vitaGL calls=%u SDL=%s",
                  (void *)renderer, driver, gl_init_calls, renderer ? "(success)" : SDL_GetError());
    log_free_memory("after native GXM");
    return renderer;
}

/* Preserve the last submitted source independently of vitaGL's info/source
 * queries, which can be empty on a compile failure. SDL compiles immediately
 * after glShaderSource; this bounded snapshot also covers that failure path. */
static GLuint submitted_shader;
static char submitted_source[16384];
static void diagnostic_shader_source(GLuint shader, GLsizei count,
                                     const GLchar *const *strings, const GLint *lengths)
{
    submitted_shader = shader;
    size_t used = 0;
    for (int i = 0; i < count && used < sizeof(submitted_source) - 1; i++) {
        if (!strings[i]) continue;
        size_t n = lengths && lengths[i] >= 0 ? (size_t)lengths[i] : strlen(strings[i]);
        if (n > sizeof(submitted_source) - used - 1) n = sizeof(submitted_source) - used - 1;
        memcpy(submitted_source + used, strings[i], n);
        used += n;
    }
    submitted_source[used] = 0;
    glShaderSource(shader, count, strings, lengths);
}

/* SDL resolves GL entry points via vglGetProcAddress, not direct linkage. */
static void diagnostic_compile_shader(GLuint shader)
{
    vita_boot_watch_stage("SDL shader compilation");
    vita_boot_log("shader compile begin id=%u", shader);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    char info[2048] = {0};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    glGetShaderInfoLog(shader, sizeof(info), NULL, info);
    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    vita_boot_log("shader compile done id=%u status=%d info_length=%d info=%s", shader, ok, log_length, info);
    if (!ok) {
        if (submitted_shader == shader)
            vita_boot_log("captured failed shader id=%u submitted source BEGIN\n%s\nsource END", shader, submitted_source);
        GLint length = 0;
        glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &length);
        if (length > 0 && length <= 65536) {
            char *source = malloc((size_t)length + 1);
            if (source) {
                source[0] = 0;
                glGetShaderSource(shader, length, NULL, source);
                source[length] = 0;
                vita_boot_log("failed shader id=%u source BEGIN\n%s\nsource END", shader, source);
                free(source);
            }
        }
        vita_boot_log("shader failure returned to SDL (allow its retry/error handling; not fatal per compile)");
    }
    vita_boot_watch_stage("SDL renderer construction after shader compilation");
}

static void diagnostic_link_program(GLuint program)
{
    vita_boot_watch_stage("SDL shader program linking");
    vita_boot_log("shader program link begin id=%u", program);
    glLinkProgram(program);
    GLint ok = GL_FALSE;
    char info[2048] = {0};
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glGetProgramInfoLog(program, sizeof(info), NULL, info);
    vita_boot_log("shader program link done id=%u status=%d info=%s", program, ok, info);
    if (!ok) vita_boot_log("shader program link failure returned to SDL for renderer-level handling");
    vita_boot_watch_stage("SDL renderer construction after shader program linking");
}

void *__real_vglGetProcAddress(const char *name);
void *__wrap_vglGetProcAddress(const char *name)
{
    if (!strcmp(name, "glShaderSource")) return (void *)diagnostic_shader_source;
    if (!strcmp(name, "glCompileShader")) return (void *)diagnostic_compile_shader;
    if (!strcmp(name, "glLinkProgram")) return (void *)diagnostic_link_program;
    return __real_vglGetProcAddress(name);
}
