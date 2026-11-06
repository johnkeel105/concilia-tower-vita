/* Vita / vitaGL helpers for the ConciliaTower port. */
#ifndef CT_VITA_PLATFORM_H
#define CT_VITA_PLATFORM_H

#include <SDL.h>

#define strcasecmp  SDL_strcasecmp
#define strncasecmp SDL_strncasecmp

#ifdef __cplusplus
extern "C" {
#endif

void vita_boost_clocks(void);
void vita_prepare_userdata(void);
void vita_try_expand_ex_(void);
void vita_init_runtime(void);
const char *vita_save_path(void);

/* Returns 1 if the event was a controller button that should be ignored.
 * Returns 0 after rewriting a mapped button into a key event, or if the
 * event was not a controller button. */
int vita_remap_event(SDL_Event *ev);
void vita_pan_camera(float *cam_fx, float *cam_fy);

/* Full-screen notice if SIMTOWER.EXE is missing. Returns 1. */
int vita_missing_exe_screen(void);

#ifdef __cplusplus
}
#endif

#endif
