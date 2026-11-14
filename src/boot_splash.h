/* LiveArea pic0 splash for the pre-game EXE check and loading frames. */
#ifndef CT_BOOT_SPLASH_INCLUDED
#define CT_BOOT_SPLASH_INCLUDED

#include <SDL.h>
#include <SDL_ttf.h>

#define CT_BOOT_SPLASH_W 960
#define CT_BOOT_SPLASH_H 544

#ifdef __cplusplus
extern "C" {
#endif

/* Decode app0:sce_sys/pic0.png (same 960x544 art as the LiveArea loading
 * picture). Safe to call more than once; 0 on success. */
int ct_boot_splash_load(void);
int ct_boot_splash_ready(void);

/* Copy the splash into a 32-bit BGRX framebuffer (pitch in pixels). */
void ct_boot_splash_blit(void *fb, int pitch_px, int fb_h);

/* 8x8 debug-font text, foreground pixels only, with a 1px drop shadow.
 * Newlines wrap; long lines wrap at max_w. Returns the height used. */
int ct_boot_splash_text(void *fb, int pitch_px, int fb_h,
                        int x, int y, int max_w, const char *text,
                        unsigned fg_rgb);

/* Blit splash and draw title + body over the art (light shadow only). */
void ct_boot_splash_notice(void *fb, int pitch_px, int fb_h,
                           const char *title, const char *body);

/* Draw the splash through SDL. If font is non-NULL, also draw "Loading..."
 * in the bottom-right with a light drop shadow (no box). */
void ct_boot_splash_present(SDL_Renderer *r, TTF_Font *font);

/* Draw the splash (and optional "Loading...") over the current framebuffer
 * at the given opacity, without clearing or presenting. Used to fade the
 * load art out over the first gameplay frame. */
void ct_boot_splash_draw(SDL_Renderer *r, TTF_Font *font, unsigned char alpha);

/* Load the first readable PNG in paths[] as an SDL texture (original size). */
SDL_Texture *ct_load_png_texture(SDL_Renderer *r, const char *const *paths);

#ifdef __cplusplus
}
#endif

#endif
