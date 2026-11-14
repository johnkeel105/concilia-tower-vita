/* Boot splash: LiveArea pic0 art under EXE-check notices and the loading frame. */
#include "boot_splash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <png.h>
#include "debugScreen.h"

void vita_boot_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define SPLASH_W CT_BOOT_SPLASH_W
#define SPLASH_H CT_BOOT_SPLASH_H
#define PX(r, g, b) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16))

static uint32_t *splash;
static SDL_Texture *splash_tex;
static SDL_Renderer *splash_tex_renderer;

int ct_boot_splash_ready(void)
{
    return splash != NULL;
}

static void pack_row(uint32_t *dst, const unsigned char *src, int src_w, int nchan)
{
    for (int x = 0; x < SPLASH_W; x++) {
        int sx = (src_w == SPLASH_W) ? x : (int)((int64_t)x * src_w / SPLASH_W);
        const unsigned char *p = src + (size_t)sx * (size_t)nchan;
        dst[x] = PX(p[0], p[1], p[2]);
    }
}

int ct_boot_splash_load(void)
{
    if (splash) return 0;

    static const char *paths[] = {
        "app0:sce_sys/pic0.png",
        "app0:/sce_sys/pic0.png",
        NULL
    };
    FILE *fp = NULL;
    const char *used = NULL;
    for (int i = 0; paths[i]; i++) {
        fp = fopen(paths[i], "rb");
        if (fp) { used = paths[i]; break; }
    }
    if (!fp) {
        vita_boot_log("boot splash: pic0.png not found under app0:sce_sys");
        return -1;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info) {
        if (png) png_destroy_read_struct(&png, info ? &info : NULL, NULL);
        fclose(fp);
        vita_boot_log("boot splash: libpng init failed");
        return -1;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        vita_boot_log("boot splash: PNG decode failed");
        return -1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);
    png_set_expand(png);
    png_set_strip_16(png);
    png_set_gray_to_rgb(png);
    png_set_packing(png);
    png_read_update_info(png, info);

    int width = (int)png_get_image_width(png, info);
    int height = (int)png_get_image_height(png, info);
    int nchan = png_get_channels(png, info);
    png_size_t rowbytes = png_get_rowbytes(png, info);
    if (width <= 0 || height <= 0 || nchan < 3 || rowbytes == 0) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        vita_boot_log("boot splash: unexpected PNG geometry %dx%d ch=%d", width, height, nchan);
        return -1;
    }

    unsigned char *row = malloc(rowbytes);
    uint32_t *pixels = malloc((size_t)SPLASH_W * (size_t)SPLASH_H * sizeof(uint32_t));
    if (!row || !pixels) {
        free(row);
        free(pixels);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        vita_boot_log("boot splash: out of memory");
        return -1;
    }

    for (int y = 0; y < height; y++) {
        png_read_row(png, row, NULL);
        int dy0 = (height == SPLASH_H) ? y : (int)((int64_t)y * SPLASH_H / height);
        int dy1 = (height == SPLASH_H) ? y + 1 : (int)((int64_t)(y + 1) * SPLASH_H / height);
        if (dy1 > SPLASH_H) dy1 = SPLASH_H;
        if (dy1 <= dy0) continue;
        pack_row(pixels + (size_t)dy0 * SPLASH_W, row, width, nchan);
        for (int dy = dy0 + 1; dy < dy1; dy++)
            memcpy(pixels + (size_t)dy * SPLASH_W, pixels + (size_t)dy0 * SPLASH_W,
                   (size_t)SPLASH_W * sizeof(uint32_t));
    }
    /* If the source is shorter than 544, pad the last row downward. */
    if (height < SPLASH_H) {
        int last = (int)((int64_t)(height - 1) * SPLASH_H / height);
        if (last < 0) last = 0;
        for (int dy = last + 1; dy < SPLASH_H; dy++)
            memcpy(pixels + (size_t)dy * SPLASH_W, pixels + (size_t)last * SPLASH_W,
                   (size_t)SPLASH_W * sizeof(uint32_t));
    }

    png_read_end(png, NULL);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    free(row);
    splash = pixels;
    vita_boot_log("boot splash: loaded %s (%dx%d -> %dx%d)", used, width, height, SPLASH_W, SPLASH_H);
    return 0;
}

void ct_boot_splash_blit(void *fb, int pitch_px, int fb_h)
{
    if (!fb || pitch_px <= 0 || fb_h <= 0) return;
    uint32_t *dst = fb;
    int h = fb_h < SPLASH_H ? fb_h : SPLASH_H;
    int w = pitch_px < SPLASH_W ? pitch_px : SPLASH_W;
    if (!splash) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                dst[y * pitch_px + x] = 0;
        return;
    }
    for (int y = 0; y < h; y++)
        memcpy(dst + y * pitch_px, splash + y * SPLASH_W, (size_t)w * sizeof(uint32_t));
}

static void plot(uint32_t *fb, int pitch_px, int fb_h, int x, int y, uint32_t c)
{
    if (x < 0 || y < 0 || x >= SPLASH_W || y >= fb_h || x >= pitch_px) return;
    fb[y * pitch_px + x] = c;
}

int ct_boot_splash_text(void *fb, int pitch_px, int fb_h,
                        int x, int y, int max_w, const char *text,
                        unsigned fg_rgb)
{
    if (!fb || !text || pitch_px <= 0 || fb_h <= 0) return 0;
    PsvDebugScreenFont *font = psvDebugScreenGetFont();
    if (!font || !font->glyphs || font->width == 0 || font->height == 0) return 0;

    int fw = font->size_w ? font->size_w : font->width;
    int fh = font->size_h ? font->size_h : font->height;
    int bits = font->width * font->height;
    uint32_t fg = PX((fg_rgb >> 16) & 255, (fg_rgb >> 8) & 255, fg_rgb & 255);
    uint32_t sh = PX(0, 0, 0);
    int origin_x = x;
    int start_y = y;
    int right = x + max_w;
    if (max_w <= 0) right = SPLASH_W - 8;

    uint32_t *dst = fb;
    const unsigned char *p = (const unsigned char *)text;
    for (; *p; p++) {
        unsigned char t = *p;
        if (t == '\n') { x = origin_x; y += fh; continue; }
        if (t == '\r') { x = origin_x; continue; }
        if (t == '\t') { x += fw * 4; continue; }
        if (x + fw > right && x > origin_x) { x = origin_x; y += fh; }
        if (y + fh > fb_h) break;

        if (t >= font->first && t <= font->last) {
            int bit0 = (t - font->first) * bits;
            const unsigned char *glyph = &font->glyphs[bit0 / 8];
            unsigned char mask = (unsigned char)(1 << 7);
            int bit = bit0 % 8;
            for (; bit > 0; bit--) mask >>= 1;
            for (int row = 0; row < font->height; row++) {
                unsigned char m = mask;
                const unsigned char *g = glyph;
                for (int col = 0; col < font->width; col++, m >>= 1) {
                    if (!m) { g++; m = (unsigned char)(1 << 7); }
                    if (*g & m) {
                        plot(dst, pitch_px, fb_h, x + col + 1, y + row + 1, sh);
                        plot(dst, pitch_px, fb_h, x + col, y + row, fg);
                    }
                }
                /* Advance to the first bit of the next row. */
                int consumed = font->width;
                while (consumed--) {
                    mask >>= 1;
                    if (!mask) { glyph++; mask = (unsigned char)(1 << 7); }
                }
            }
        }
        x += fw;
    }
    return (y + fh) - start_y;
}

void ct_boot_splash_notice(void *fb, int pitch_px, int fb_h,
                           const char *title, const char *body)
{
    ct_boot_splash_load();
    ct_boot_splash_blit(fb, pitch_px, fb_h);
    int y = 20;
    if (title && *title)
        y += ct_boot_splash_text(fb, pitch_px, fb_h, 20, y, 920, title, 0xFFFFF0) + 8;
    if (body && *body)
        ct_boot_splash_text(fb, pitch_px, fb_h, 20, y, 920, body, 0xF0F0F0);
}

static SDL_Texture *splash_texture(SDL_Renderer *r)
{
    if (!r) return NULL;
    ct_boot_splash_load();
    if (!splash) return NULL;
    if (splash_tex && splash_tex_renderer == r) return splash_tex;
    if (splash_tex) { SDL_DestroyTexture(splash_tex); splash_tex = NULL; }
    SDL_Surface *s = SDL_CreateRGBSurfaceFrom(splash, SPLASH_W, SPLASH_H, 32,
                                              SPLASH_W * 4,
                                              0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0);
    if (!s) return NULL;
    splash_tex = SDL_CreateTextureFromSurface(r, s);
    SDL_FreeSurface(s);
    splash_tex_renderer = r;
    return splash_tex;
}

void ct_boot_splash_draw(SDL_Renderer *r, TTF_Font *font, unsigned char alpha)
{
    if (!r || alpha == 0) return;
    SDL_Texture *tex = splash_texture(r);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(tex, alpha);
        SDL_RenderCopy(r, tex, NULL, NULL);
        SDL_SetTextureAlphaMod(tex, 255);
    }
    if (font) {
        SDL_Color white = { 230, 230, 230, 255 };
        SDL_Color black = { 0, 0, 0, 255 };
        SDL_Surface *fg = TTF_RenderUTF8_Blended(font, "Loading...", white);
        SDL_Surface *sh = TTF_RenderUTF8_Blended(font, "Loading...", black);
        int lw = SPLASH_W, lh = SPLASH_H;
        if (fg) {
            SDL_Rect d = { lw - fg->w - 14, lh - fg->h - 10, fg->w, fg->h };
            if (sh) {
                SDL_Texture *st = SDL_CreateTextureFromSurface(r, sh);
                if (st) {
                    SDL_SetTextureBlendMode(st, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureAlphaMod(st, alpha);
                    SDL_Rect sd = { d.x + 1, d.y + 1, d.w, d.h };
                    SDL_RenderCopy(r, st, NULL, &sd);
                    SDL_DestroyTexture(st);
                }
                SDL_FreeSurface(sh);
            }
            SDL_Texture *ft = SDL_CreateTextureFromSurface(r, fg);
            if (ft) {
                SDL_SetTextureBlendMode(ft, SDL_BLENDMODE_BLEND);
                SDL_SetTextureAlphaMod(ft, alpha);
                SDL_RenderCopy(r, ft, NULL, &d);
                SDL_DestroyTexture(ft);
            }
            SDL_FreeSurface(fg);
        } else if (sh) {
            SDL_FreeSurface(sh);
        }
    }
}

void ct_boot_splash_present(SDL_Renderer *r, TTF_Font *font)
{
    if (!r) return;
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    ct_boot_splash_draw(r, font, 255);
    SDL_RenderPresent(r);
}

SDL_Texture *ct_load_png_texture(SDL_Renderer *r, const char *const *paths)
{
    if (!r || !paths) return NULL;
    FILE *fp = NULL;
    const char *used = NULL;
    for (int i = 0; paths[i]; i++) {
        fp = fopen(paths[i], "rb");
        if (fp) { used = paths[i]; break; }
    }
    if (!fp) return NULL;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info) {
        if (png) png_destroy_read_struct(&png, info ? &info : NULL, NULL);
        fclose(fp);
        return NULL;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        vita_boot_log("png texture: decode failed (%s)", used);
        return NULL;
    }
    png_init_io(png, fp);
    png_read_info(png, info);
    png_set_expand(png);
    png_set_strip_16(png);
    png_set_gray_to_rgb(png);
    png_set_packing(png);
    png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
    png_read_update_info(png, info);

    int width = (int)png_get_image_width(png, info);
    int height = (int)png_get_image_height(png, info);
    int nchan = png_get_channels(png, info);
    png_size_t rowbytes = png_get_rowbytes(png, info);
    if (width <= 0 || height <= 0 || nchan < 4 || rowbytes == 0) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }
    unsigned char *pixels = malloc((size_t)height * rowbytes);
    if (!pixels) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }
    png_bytep *rows = malloc((size_t)height * sizeof(*rows));
    if (!rows) {
        free(pixels);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return NULL;
    }
    for (int y = 0; y < height; y++)
        rows[y] = pixels + (size_t)y * rowbytes;
    png_read_image(png, rows);
    png_read_end(png, NULL);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    free(rows);

    SDL_Surface *s = SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, (int)rowbytes,
                                              0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0xFF000000u);
    if (!s) { free(pixels); return NULL; }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, s);
    SDL_FreeSurface(s);
    free(pixels);
    if (tex) {
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
#endif
        vita_boot_log("png texture: loaded %s (%dx%d)", used, width, height);
    }
    return tex;
}
