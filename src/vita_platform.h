/* Vita native-GXM boot, input and frame diagnostics. */
#ifndef CT_VITA_PLATFORM_H
#define CT_VITA_PLATFORM_H

#include <SDL.h>
#include "vita_input.h"

#define strcasecmp  SDL_strcasecmp
#define strncasecmp SDL_strncasecmp

#ifdef __cplusplus
extern "C" {
#endif

void vita_boot_begin(void);
void vita_boot_watch_stage(const char *stage);
SDL_Renderer *vita_create_native_renderer(SDL_Window *window);
void vita_boot_notice(const char *message);
void vita_boot_notice_end(void);
void vita_boot_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int vita_boot_fatal(const char *reason);
int vita_probe_file(const char *path);
int vita_shader_library_present(void);
void vita_boost_clocks(void);
void vita_prepare_userdata(void);
void vita_try_expand_ex_(void);
void vita_init_runtime(void);
const char *vita_save_path(void);
int vita_set_save_name(const char *name);

void vita_input_hints(void);
void vita_input_event(const SDL_Event *,CT_InputSink);
int vita_update_input(double dt,float *,float *,CT_InputSink);
void vita_draw_cursor(SDL_Renderer *);
void vita_present(SDL_Renderer *);
void vita_frame_finish(Uint64 start,double dt,double sim_ms,double render_ms);

/* Full-screen notice if SIMTOWER.EXE is missing. Returns 1. */
int vita_missing_exe_screen(void);

#ifdef __cplusplus
}
#endif

#endif
