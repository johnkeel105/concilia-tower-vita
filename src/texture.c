#include "texture.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef __vita__
#include "vita_platform.h"
#define TRACE(...) vita_boot_log(__VA_ARGS__)
#else
#define TRACE(...) SDL_Log(__VA_ARGS__)
#endif

typedef struct { SDL_Texture *gpu; SDL_Rect rect; } Tile;
struct CT_Texture {
    SDL_Renderer *renderer;
    int w, h, count, access, refs;
    Tile *tiles;
    SDL_Surface *pixels; /* Only oversized textures retain a CPU resampling source. */
    struct CT_Texture *scaled; /* Detached, owned cache for fractional cross-tile draws. */
    SDL_Rect cached_source;
    int cached_w, cached_h;
    Uint8 red, green, blue, alpha;
    SDL_BlendMode blend;
    SDL_ScaleMode scale;
    struct CT_Texture *next;
};
static CT_Texture *textures;
static CT_TextureStats texture_stats;
CT_TextureStats CT_TextureStatsReset(void) { CT_TextureStats s=texture_stats; memset(&texture_stats,0,sizeof(texture_stats)); return s; }
CT_Texture *CT_RetainTexture(CT_Texture *t) { if(t) t->refs++; return t; }
static int test_width, test_height, fail_next_upload;
void CT_TextureTestLimit(int w, int h) { test_width = w; test_height = h; }
void CT_TextureTestFailNextUpload(void) { fail_next_upload = 1; }
int CT_TextureIsPlaceholder(const CT_Texture *t) { return !t || !t->count; }
int CT_TextureTileCount(const CT_Texture *t) { return t ? t->count : 0; }

static void limits(SDL_Renderer *r, int *w, int *h)
{
    SDL_RendererInfo info;
    *w = *h = INT_MAX;
    if (SDL_GetRendererInfo(r, &info) == 0) {
        if (info.max_texture_width > 0) *w = info.max_texture_width;
        if (info.max_texture_height > 0) *h = info.max_texture_height;
    }
    if (test_width > 0 && test_width < *w) *w = test_width;
    if (test_height > 0 && test_height < *h) *h = test_height;
}

static void discard_tiles(CT_Texture *t)
{
    for (int i = 0; i < t->count; i++) SDL_DestroyTexture(t->tiles[i].gpu);
    free(t->tiles); t->tiles = NULL; t->count = 0;
}

static CT_Texture *allocate(SDL_Renderer *r, int w, int h, int access)
{
    if (!r || w <= 0 || h <= 0) { SDL_SetError("Invalid logical texture size"); return NULL; }
    CT_Texture *t = calloc(1, sizeof(*t));
    if (!t) { SDL_OutOfMemory(); return NULL; }
    t->refs=1;
    t->renderer = r; t->w = w; t->h = h; t->access = access;
    t->red = t->green = t->blue = t->alpha = 255;
    t->blend = SDL_BLENDMODE_NONE;
    t->next = textures; textures = t;
    return t;
}

static int prepare_tiles(CT_Texture *t, int mw, int mh)
{
    size_t cols = 1 + (t->w - 1) / mw, rows = 1 + (t->h - 1) / mh;
    if (cols > INT_MAX / rows || cols * rows > SIZE_MAX / sizeof(Tile)) return SDL_OutOfMemory();
    t->tiles = calloc(cols * rows, sizeof(Tile));
    if (!t->tiles) return SDL_OutOfMemory();
    int i = 0;
    for (int y = 0; y < t->h; ) {
        int h = SDL_min(mh, t->h - y);
        for (int x = 0; x < t->w; ) {
            int w = SDL_min(mw, t->w - x);
            t->tiles[i++].rect = (SDL_Rect){x, y, w, h};
            x += w;
        }
        y += h;
    }
    t->count = i;
    if (i > 1) TRACE("texture tile plan: logical=%dx%d renderer limit=%dx%d tiles=%d", t->w, t->h, mw, mh, i);
    return 0;
}

CT_Texture *CT_CreateTextureFromSurface(SDL_Renderer *r, SDL_Surface *s)
{
    if (!s) { SDL_SetError("NULL texture surface"); return NULL; }
    CT_Texture *t = allocate(r, s->w, s->h, SDL_TEXTUREACCESS_STATIC);
    if (!t) return NULL;
    int mw, mh; limits(r, &mw, &mh);
    SDL_Surface *rgba = NULL;
    if (prepare_tiles(t, mw, mh) < 0) goto degraded;
    if (fail_next_upload) { fail_next_upload = 0; SDL_SetError("Test-injected texture upload failure"); goto degraded; }
    /* Fast path preserves SDL's exact handling of surface mods and color keys. */
    if (t->count == 1) {
        t->tiles[0].gpu = SDL_CreateTextureFromSurface(r, s);
        if (!t->tiles[0].gpu) goto degraded;
    } else {
        /* Convert a color key to alpha once. Cropped views share pixels, so no
         * giant GPU allocation or per-tile CPU copy is needed. Conversion also
         * supports indexed/24-bit fonts or resources, not just ARGB sprites. */
        rgba = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ARGB8888, 0);
        if (!rgba) goto degraded;
        Uint32 key;
        if (SDL_HasColorKey(rgba) && SDL_GetColorKey(rgba, &key) == 0) {
            if (SDL_LockSurface(rgba) < 0) goto degraded;
            for (int y = 0; y < rgba->h; y++) {
                Uint32 *row = (Uint32 *)((Uint8 *)rgba->pixels + y * rgba->pitch);
                for (int x = 0; x < rgba->w; x++) if (row[x] == key) row[x] &= 0x00ffffffu;
            }
            SDL_UnlockSurface(rgba);
            SDL_SetColorKey(rgba, SDL_FALSE, 0);
            SDL_SetSurfaceBlendMode(rgba, SDL_BLENDMODE_BLEND);
        }
        Uint8 red, green, blue, alpha; SDL_BlendMode blend;
        SDL_GetSurfaceColorMod(rgba, &red, &green, &blue);
        SDL_GetSurfaceAlphaMod(rgba, &alpha);
        SDL_GetSurfaceBlendMode(rgba, &blend);
        if (SDL_LockSurface(rgba) < 0) goto degraded;
        for (int i = 0; i < t->count; i++) {
            SDL_Rect a = t->tiles[i].rect;
            SDL_Surface *view = SDL_CreateRGBSurfaceWithFormatFrom(
                (Uint8 *)rgba->pixels + a.y * rgba->pitch + a.x * 4,
                a.w, a.h, 32, rgba->pitch, SDL_PIXELFORMAT_ARGB8888);
            if (view) {
                SDL_SetSurfaceColorMod(view, red, green, blue);
                SDL_SetSurfaceAlphaMod(view, alpha);
                SDL_SetSurfaceBlendMode(view, blend);
                t->tiles[i].gpu = SDL_CreateTextureFromSurface(r, view);
                SDL_FreeSurface(view);
            }
            if (!t->tiles[i].gpu) { SDL_UnlockSurface(rgba); goto degraded; }
        }
        SDL_UnlockSurface(rgba);
        t->pixels = rgba; rgba = NULL;
        SDL_SetSurfaceColorMod(t->pixels, 255, 255, 255);
        SDL_SetSurfaceAlphaMod(t->pixels, 255);
        SDL_SetSurfaceBlendMode(t->pixels, SDL_BLENDMODE_NONE);
    }
    texture_stats.uploads += t->count;
    SDL_GetTextureScaleMode(t->tiles[0].gpu, &t->scale);
    SDL_GetTextureBlendMode(t->tiles[0].gpu, &t->blend);
    SDL_GetTextureColorMod(t->tiles[0].gpu, &t->red, &t->green, &t->blue);
    SDL_GetTextureAlphaMod(t->tiles[0].gpu, &t->alpha);
    return t;
degraded:
    TRACE("texture upload degraded: logical=%dx%d SDL=%s; keeping visible placeholder, not exiting", t->w, t->h, SDL_GetError());
    SDL_FreeSurface(rgba);
    discard_tiles(t);
    return t;
}

CT_Texture *CT_CreateTexture(SDL_Renderer *r, Uint32 format, int access, int w, int h)
{
    int mw, mh; limits(r, &mw, &mh);
    /* Render targets are viewport-sized, not EXE strips. Rendering to a virtual
     * multi-target would require replaying all SDL primitives. Reject this
     * optional effect safely instead of silently drawing into a partial target. */
    if (access == SDL_TEXTUREACCESS_TARGET && (w > mw || h > mh)) {
        TRACE("optional target %dx%d exceeds %dx%d; effect disabled", w, h, mw, mh);
        SDL_SetError("Render target exceeds renderer viewport limit"); return NULL;
    }
    CT_Texture *t = allocate(r, w, h, access);
    if (!t) return NULL;
    if (prepare_tiles(t, mw, mh) < 0) { CT_DestroyTexture(t); return NULL; }
    for (int i = 0; i < t->count; i++) {
        SDL_Rect a = t->tiles[i].rect;
        t->tiles[i].gpu = SDL_CreateTexture(r, format, access, a.w, a.h);
        if (!t->tiles[i].gpu) { CT_DestroyTexture(t); return NULL; }
    }
    return t;
}

void CT_DestroyTexture(CT_Texture *t)
{
    if (!t || --t->refs > 0) return;
    CT_Texture **p = &textures;
    while (*p && *p != t) p = &(*p)->next;
    if (*p) *p = t->next;
    CT_DestroyTexture(t->scaled);
    SDL_FreeSurface(t->pixels);
    discard_tiles(t); free(t);
}
void CT_DestroyTextures(SDL_Renderer *r)
{
    CT_Texture *t = textures;
    while (t) { CT_Texture *next = t->next; if (t->renderer == r) { t->refs=1; CT_DestroyTexture(t); } t = next; }
}

int CT_RenderCopy(SDL_Renderer *r, CT_Texture *t, const SDL_Rect *source, const SDL_Rect *dest)
{
    if (!t || t->renderer != r) return SDL_SetError("Invalid logical texture/renderer");
    SDL_Rect src = source ? *source : (SDL_Rect){0, 0, t->w, t->h};
    SDL_Rect bounds = {0, 0, t->w, t->h};
    if (!SDL_IntersectRect(&src, &bounds, &src)) return 0;
    SDL_Rect dst;
    if (dest) dst = *dest;
    else SDL_RenderGetViewport(r, &dst), dst.x = dst.y = 0;
    if (src.w <= 0 || src.h <= 0 || dst.w <= 0 || dst.h <= 0) return 0;
    if (!t->count) {
        Uint8 red, green, blue, alpha; SDL_BlendMode blend;
        SDL_GetRenderDrawColor(r, &red, &green, &blue, &alpha);
        SDL_GetRenderDrawBlendMode(r, &blend);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, t->red, 0, t->blue, t->alpha);
        int rc = SDL_RenderFillRect(r, &dst);
        SDL_SetRenderDrawColor(r, red, green, blue, alpha);
        SDL_SetRenderDrawBlendMode(r, blend);
        return rc;
    }
    if (t->count == 1) return SDL_RenderCopy(r, t->tiles[0].gpu, source, dest);
    /* A frame contained in one tile takes SDL's exact ordinary scaling path. */
    for (int i = 0; i < t->count; i++) {
        SDL_Rect a = t->tiles[i].rect;
        if (src.x >= a.x && src.y >= a.y &&
            (int64_t)src.x+src.w <= (int64_t)a.x+a.w &&
            (int64_t)src.y+src.h <= (int64_t)a.y+a.h) {
            SDL_Rect local = {src.x-a.x, src.y-a.y, src.w, src.h};
            return SDL_RenderCopy(r, t->tiles[i].gpu, &local, dest);
        }
    }
    /* SDL's software backend rounds fractional split destination edges and
     * restarts the sampling phase per tile. For a scaled cross-boundary draw,
     * cache a bounded CPU crop for SDL to scale as one image. If the requested
     * source itself is oversized, resample first and tile its result at 1:1.
     * The last-draw cache avoids re-uploading identical draws every frame. */
    if (t->pixels && (dst.w % src.w || dst.h % src.h)) {
        if (!t->scaled || memcmp(&t->cached_source, &src, sizeof(src)) ||
            t->cached_w != dst.w || t->cached_h != dst.h) {
            texture_stats.scaled_cache_misses++;
            CT_DestroyTexture(t->scaled); t->scaled = NULL;
            int mw, mh; limits(r, &mw, &mh);
            int crop_fits = src.w <= mw && src.h <= mh;
            SDL_Surface *scaled = SDL_CreateRGBSurfaceWithFormat(0,
                crop_fits ? src.w : dst.w, crop_fits ? src.h : dst.h,
                32, SDL_PIXELFORMAT_ARGB8888);
            if (!scaled) return -1;
            int rc = crop_fits ? SDL_BlitSurface(t->pixels, &src, scaled, NULL) :
                t->scale == SDL_ScaleModeLinear ? SDL_SoftStretchLinear(t->pixels, &src, scaled, NULL) :
                SDL_BlitScaled(t->pixels, &src, scaled, NULL);
            if (rc < 0) { SDL_FreeSurface(scaled); return -1; }
            t->scaled = CT_CreateTextureFromSurface(r, scaled);
            SDL_FreeSurface(scaled);
            if (!t->scaled) return -1;
            /* Cache has exactly one owner; keep it off the renderer cleanup list. */
            CT_Texture **p = &textures;
            while (*p && *p != t->scaled) p = &(*p)->next;
            if (*p) *p = t->scaled->next;
            t->cached_source = src; t->cached_w = dst.w; t->cached_h = dst.h;
        }
        CT_SetTextureScaleMode(t->scaled, t->scale);
        CT_SetTextureColorMod(t->scaled, t->red, t->green, t->blue);
        CT_SetTextureAlphaMod(t->scaled, t->alpha);
        CT_SetTextureBlendMode(t->scaled, t->blend);
        return CT_RenderCopy(r, t->scaled, NULL, &dst);
    }
    for (int i = 0; i < t->count; i++) {
        SDL_Rect piece;
        Tile *tile = &t->tiles[i];
        if (!SDL_IntersectRect(&src, &tile->rect, &piece)) continue;
        /* Shared floating-point edges preserve global source/destination scale,
         * including clipped sources and a frame straddling a tile boundary. */
        double x0 = dst.x + (double)(piece.x - src.x) * dst.w / src.w;
        double y0 = dst.y + (double)(piece.y - src.y) * dst.h / src.h;
        double x1 = dst.x + (double)(piece.x + piece.w - src.x) * dst.w / src.w;
        double y1 = dst.y + (double)(piece.y + piece.h - src.y) * dst.h / src.h;
        SDL_FRect out = {(float)x0, (float)y0, (float)(x1-x0), (float)(y1-y0)};
        piece.x -= tile->rect.x; piece.y -= tile->rect.y;
        texture_stats.tile_draws++;
        if (SDL_RenderCopyF(r, tile->gpu, &piece, &out) < 0) return -1;
    }
    return 0;
}

int CT_SetTextureAlphaMod(CT_Texture *t, Uint8 a)
{
    if (!t) return -1;
    t->alpha = a;
    for (int i = 0; i < t->count; i++) if (SDL_SetTextureAlphaMod(t->tiles[i].gpu, a) < 0) return -1;
    return 0;
}
int CT_SetTextureColorMod(CT_Texture *t, Uint8 r, Uint8 g, Uint8 b)
{
    if (!t) return -1;
    t->red = r; t->green = g; t->blue = b;
    for (int i = 0; i < t->count; i++) if (SDL_SetTextureColorMod(t->tiles[i].gpu, r, g, b) < 0) return -1;
    return 0;
}
int CT_SetTextureBlendMode(CT_Texture *t, SDL_BlendMode b)
{
    if (!t) return -1;
    t->blend = b;
    for (int i = 0; i < t->count; i++) if (SDL_SetTextureBlendMode(t->tiles[i].gpu, b) < 0) return -1;
    return 0;
}
int CT_SetTextureScaleMode(CT_Texture *t, SDL_ScaleMode mode)
{
    if (!t) return -1;
    if (t->scale != mode) { CT_DestroyTexture(t->scaled); t->scaled = NULL; }
    t->scale = mode;
    for (int i = 0; i < t->count; i++) if (SDL_SetTextureScaleMode(t->tiles[i].gpu, mode) < 0) return -1;
    return 0;
}
int CT_SetRenderTarget(SDL_Renderer *r, CT_Texture *t)
{
    if (!t) return SDL_SetRenderTarget(r, NULL);
    if (t->renderer != r || t->access != SDL_TEXTUREACCESS_TARGET || t->count != 1)
        return SDL_SetError("Invalid logical render target");
    return SDL_SetRenderTarget(r, t->tiles[0].gpu);
}
