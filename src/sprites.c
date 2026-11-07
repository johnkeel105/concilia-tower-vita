/* sprites.c - Sprite atlas implementation */
#include "sprites.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef __vita__
#include "vita_platform.h"
#define SPR_TRACE(...) vita_boot_log(__VA_ARGS__)
#else
#define SPR_TRACE(...) ((void)0)
#endif

/* Explicit little-endian loads also avoid unaligned pointer casts on ARM. */
static uint32_t dib_u32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t dib_u16(const uint8_t *p)
{ return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

/* ---------- Palette loading ---------- */
static int load_palette(SpriteAtlas *atlas, NEResourceTable *exe)
{
    NEResource *pal = ne_find(exe, 0xFF03, 0x83e8);
    if (!pal) return -1;
    
    int count = pal->length / 8;
    if (count > 256) count = 256;
    
    for (int i = 0; i < count; i++) {
        uint8_t *p = pal->data + i * 8;
        atlas->palette[i][0] = p[2]; /* R */
        atlas->palette[i][1] = p[4]; /* G */
        atlas->palette[i][2] = p[6]; /* B */
    }
    
    atlas->palette_loaded = 1;
    return 0;
}

/* ---------- DIB → SDL_Surface ---------- */
SDL_Surface *dib_to_surface(NEResource *res, SpriteAtlas *atlas)
{
    if (!res || res->length < 40) return NULL;
    
    uint8_t *data = res->data;
    uint32_t hdr_size = dib_u32(data);
    int32_t width = (int32_t)dib_u32(data + 4);
    int32_t height = (int32_t)dib_u32(data + 8);
    uint16_t bpp = dib_u16(data + 14);
    
    if (hdr_size < 40 || hdr_size > (uint32_t)res->length || bpp != 8 ||
        width <= 0 || width > INT_MAX - 3 || height == 0 || height == INT_MIN)
        return NULL;
    
    int abs_height = height < 0 ? -height : height;
    int top_down = height < 0;
    
    uint8_t *color_table = data + hdr_size;
    int row_bytes = (width + 3) & ~3;
    uint8_t *pixels = color_table + 256 * 4;
    
    /* Check if we have enough data for local palette + pixels */
    uint64_t pixel_bytes = (uint64_t)row_bytes * abs_height;
    if ((uint64_t)hdr_size + pixel_bytes > (uint64_t)res->length) {
        SPR_TRACE("DIB truncated: id=0x%04x size=%d dims=%dx%d", res->id, res->length, width, height);
        return NULL;
    }
    int has_local_palette = ((uint64_t)res->length >= hdr_size + 256 * 4 + pixel_bytes);
    if (!has_local_palette) {
        pixels = color_table; /* No room for local palette */
    }
    
    SDL_Surface *surf = SDL_CreateRGBSurface(0, width, abs_height, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surf) return NULL;
    
    SDL_LockSurface(surf);
    for (int y = 0; y < abs_height; y++) {
        int src_y = top_down ? y : (abs_height - 1 - y);
        uint8_t *src_row = pixels + src_y * row_bytes;
        uint32_t *dst_row = (uint32_t *)((uint8_t *)surf->pixels + y * surf->pitch);
        
        for (int x = 0; x < width; x++) {
            uint8_t idx = src_row[x];
            if (has_local_palette) {
                uint8_t *ct = color_table + idx * 4;
                dst_row[x] = (0xFFu << 24) | (ct[2] << 16) | (ct[1] << 8) | ct[0];
            } else if (atlas->palette_loaded) {
                dst_row[x] = (0xFFu << 24) |
                    (atlas->palette[idx][0] << 16) | 
                    (atlas->palette[idx][1] << 8) | 
                    atlas->palette[idx][2];
            } else {
                dst_row[x] = (0xFFu << 24) | (idx << 16) | (idx << 8) | idx;
            }
        }
    }
    SDL_UnlockSurface(surf);
    return surf;
}

/* ---------- Raw bitmap → SDL_Surface ---------- */
static SDL_Surface *raw_to_surface(NEResource *res, SpriteAtlas *atlas)
{
    if (!res || !atlas->palette_loaded) return NULL;
    
    int cell_pixels = 36 * 8;
    int cell_count = res->length / cell_pixels;
    if (cell_count <= 0) return NULL;
    
    int width = cell_count * 8;
    int height = 36;
    
    SDL_Surface *surf = SDL_CreateRGBSurface(0, width, height, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surf) return NULL;
    
    SDL_LockSurface(surf);
    uint8_t *src = res->data;
    for (int i = 0; i < res->length; i++) {
        int srcx = i % 8;
        int srcy = i / 8;
        int dstx = srcx + (srcy / 36) * 8;
        int dsty = srcy % 36;  /* No flip — SDL surfaces are top-down */
        
        if (dstx < width && dsty < height) {
            uint8_t *c = atlas->palette[src[i]];
            uint32_t *row = (uint32_t *)((uint8_t *)surf->pixels + dsty * surf->pitch);
            row[dstx] = (0xFFu << 24) | (c[0] << 16) | (c[1] << 8) | c[2];
        }
    }
    SDL_UnlockSurface(surf);
    return surf;
}

/* ---------- Public API ---------- */

int sprites_init(SpriteAtlas *atlas, NEResourceTable *exe, SDL_Renderer *renderer)
{
    memset(atlas, 0, sizeof(*atlas));
    
    if (load_palette(atlas, exe) != 0) {
        fprintf(stderr, "sprites: warning: no palette found\n");
    }
    
    int loaded = 0, upload_failures = 0;
    
    /* Load DIB bitmaps (type 0x8002) */
    NEResourceList *dibs = ne_find_type(exe, 0x8002);
    if (dibs) {
        for (int i = 0; i < dibs->count && atlas->count < MAX_SPRITES; i++) {
            SDL_Surface *surf = dib_to_surface(&dibs->items[i], atlas);
            if (surf) {
                CT_Texture *tex = CT_CreateTextureFromSurface(renderer, surf);
                if (tex) {
                    if (CT_TextureIsPlaceholder(tex)) {
                        upload_failures++;
                        SPR_TRACE("sprite id=0x%04x degraded to placeholder", dibs->items[i].id);
                    }
                    Sprite *s = &atlas->sprites[atlas->count++];
                    s->id = dibs->items[i].id;
                    s->type = 0x8002;
                    s->texture = tex;
                    s->w = surf->w;
                    s->h = surf->h;
                    s->pixels = surf;
                    surf = NULL; /* atlas owns the source surface */
                    loaded++;
                } else {
                    upload_failures++;
                    SPR_TRACE("sprite texture upload failed: size=%dx%d SDL=%s", surf->w, surf->h, SDL_GetError());
                }
                SDL_FreeSurface(surf);
            }
        }
    }
    
    /* Load raw bitmaps (type 0xFF02) */
    NEResourceList *raws = ne_find_type(exe, 0xFF02);
    if (raws) {
        for (int i = 0; i < raws->count && atlas->count < MAX_SPRITES; i++) {
            SDL_Surface *surf = raw_to_surface(&raws->items[i], atlas);
            if (surf) {
                CT_Texture *tex = CT_CreateTextureFromSurface(renderer, surf);
                if (tex) {
                    if (CT_TextureIsPlaceholder(tex)) {
                        upload_failures++;
                        SPR_TRACE("raw sprite id=0x%04x degraded to placeholder", raws->items[i].id);
                    }
                    Sprite *s = &atlas->sprites[atlas->count++];
                    s->id = raws->items[i].id;
                    s->type = 0xFF02;
                    s->texture = tex;
                    s->w = surf->w;
                    s->h = surf->h;
                    s->pixels = surf;
                    surf = NULL; /* atlas owns the source surface */
                    loaded++;
                } else {
                    upload_failures++;
                    SPR_TRACE("sprite texture upload failed: size=%dx%d SDL=%s", surf->w, surf->h, SDL_GetError());
                }
                SDL_FreeSurface(surf);
            }
        }
    }
    
    printf("Sprites: loaded %d textures (%d DIB + %d raw)\n", 
           loaded, dibs ? dibs->count : 0, raws ? raws->count : 0);
    SPR_TRACE("sprite texture upload result: loaded=%d failed=%d", loaded, upload_failures);
    return loaded ? 0 : -1; /* A single missing cosmetic asset must not abort startup. */
}

Sprite *sprites_find(SpriteAtlas *atlas, uint16_t id)
{
    for (int i = 0; i < atlas->count; i++) {
        if (atlas->sprites[i].id == id)
            return &atlas->sprites[i];
    }
    return NULL;
}

SDL_Surface *sprites_dib_to_surface(SpriteAtlas *atlas, NEResource *res)
{
    return dib_to_surface(res, atlas);
}

void sprites_draw(SDL_Renderer *renderer, Sprite *sprite, int x, int y, SDL_Rect *src_rect)
{
    if (!sprite || !sprite->texture) return;
    
    int w = src_rect ? src_rect->w : sprite->w;
    int h = src_rect ? src_rect->h : sprite->h;
    SDL_Rect dst = { x, y, w, h };
    CT_RenderCopy(renderer, sprite->texture, src_rect, &dst);
}

void sprites_draw_scaled(SDL_Renderer *renderer, Sprite *sprite, 
                         int x, int y, int w, int h, SDL_Rect *src_rect)
{
    if (!sprite || !sprite->texture) return;
    SDL_Rect dst = { x, y, w, h };
    CT_RenderCopy(renderer, sprite->texture, src_rect, &dst);
}

/* ---------- Sprite composition ---------- */

/* Native GXM cannot read render-target textures. Keep original CPU pixels
 * instead of GPU round-tripping every transparency mask/composite operation. */
static SDL_Surface *sprite_to_surface(const Sprite *sprite)
{
    if (!sprite || !sprite->pixels) return NULL;
    return SDL_ConvertSurface(sprite->pixels, sprite->pixels->format, 0);
}

int sprites_replace_surface(Sprite *sprite, SDL_Renderer *renderer, SDL_Surface *surface)
{
    if (!sprite || !surface) return -1;
    SDL_Surface *copy = SDL_ConvertSurface(surface, surface->format, 0);
    if (!copy) return -1;
    CT_Texture *texture = CT_CreateTextureFromSurface(renderer, surface);
    if (!texture || CT_TextureIsPlaceholder(texture)) {
        CT_DestroyTexture(texture); SDL_FreeSurface(copy); return -1;
    }
    CT_DestroyTexture(sprite->texture);
    SDL_FreeSurface(sprite->pixels);
    sprite->texture = texture;
    sprite->pixels = copy;
    sprite->w = surface->w;
    sprite->h = surface->h;
    return 0;
}

/* Palette-cycle animation (the EXE's 'animated bitmaps'): frames are made
 * by cycling color-table entries in place — 197<->198, 199<->200, and a
 * 3-rotation of 201->202->203. One call = one step; registering steps 1
 * and 2 of a sheet gives its other two animation frames (step 3 = base).
 * Used by the elevator engines, security office, cinema marquee. */
int sprites_load_palette_cycled(SpriteAtlas *atlas, NEResourceTable *exe,
                                SDL_Renderer *renderer, uint16_t id,
                                uint16_t new_id, int steps)
{
    if (atlas->count >= MAX_SPRITES) return -1;
    NEResource *res = ne_find(exe, 0x8002, id);
    if (!res || res->length < 40) return -1;

    /* Copy the resource and cycle the embedded color table */
    uint8_t *copy = malloc(res->length);
    if (!copy) return -1;
    memcpy(copy, res->data, res->length);
    uint32_t hdr_size = dib_u32(copy);
    if (hdr_size < 40 || (uint64_t)hdr_size + 256 * 4 > (uint64_t)res->length) {
        free(copy);
        return -1;
    }
    uint8_t *ct = copy + hdr_size;
    for (int s = 0; s < steps; s++) {
        for (int n = 0; n < 4; n++) {
            uint8_t *o = ct + 197 * 4 + n;
            uint8_t t;
            t = o[4];  o[4]  = o[0];  o[0]  = t;          /* 197<->198 */
            t = o[12]; o[12] = o[8];  o[8]  = t;          /* 199<->200 */
            t = o[24]; o[24] = o[20]; o[20] = o[16]; o[16] = t; /* 201..203 */
        }
    }

    NEResource tmp = *res;
    tmp.data = copy;
    SDL_Surface *surf = dib_to_surface(&tmp, atlas);
    free(copy);
    if (!surf) return -1;
    CT_Texture *tex = CT_CreateTextureFromSurface(renderer, surf);
    int w = surf->w, h = surf->h;
    if (!tex) { SDL_FreeSurface(surf); return -1; }
    Sprite *sp = &atlas->sprites[atlas->count++];
    sp->id = new_id;
    sp->type = 0x8002;
    sp->texture = tex;
    sp->pixels = surf;
    sp->w = w;
    sp->h = h;
    return 0;
}

int sprites_apply_color_key(SpriteAtlas *atlas, SDL_Renderer *renderer,
                            uint16_t id, uint8_t r, uint8_t g, uint8_t b)
{
    Sprite *s = sprites_find(atlas, id);
    if (!s) return -1;
    SDL_Surface *surf = sprite_to_surface(s);
    if (!surf) return -1;
    SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(surf->format, r, g, b));
    CT_Texture *tex = CT_CreateTextureFromSurface(renderer, surf);
    if (!tex) { SDL_FreeSurface(surf); return -1; }
    CT_DestroyTexture(s->texture);
    SDL_FreeSurface(s->pixels);
    s->pixels = surf;
    s->texture = tex;
    return 0;
}

int sprites_apply_white_key(SpriteAtlas *atlas, SDL_Renderer *renderer,
                            uint16_t id)
{
    return sprites_apply_color_key(atlas, renderer, id, 0xFF, 0xFF, 0xFF);
}

int sprites_compose_h(SpriteAtlas *atlas, SDL_Renderer *renderer,
                      uint16_t id_left, uint16_t id_right, uint16_t new_id)
{
    Sprite *left = sprites_find(atlas, id_left);
    Sprite *right = sprites_find(atlas, id_right);
    if (!left || !right || atlas->count >= MAX_SPRITES) return -1;
    
    int w = left->w + right->w;
    int h = left->h > right->h ? left->h : right->h;
    
    SDL_Surface *lsurf = sprite_to_surface(left);
    SDL_Surface *rsurf = sprite_to_surface(right);
    if (!lsurf || !rsurf) {
        if (lsurf) SDL_FreeSurface(lsurf);
        if (rsurf) SDL_FreeSurface(rsurf);
        return -1;
    }
    
    SDL_Surface *combined = SDL_CreateRGBSurface(0, w, h, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!combined) { SDL_FreeSurface(lsurf); SDL_FreeSurface(rsurf); return -1; }
    SDL_FillRect(combined, NULL, 0);
    SDL_Rect ldst = { 0, 0, left->w, left->h };
    SDL_Rect rdst = { left->w, 0, right->w, right->h };
    SDL_BlitSurface(lsurf, NULL, combined, &ldst);
    SDL_BlitSurface(rsurf, NULL, combined, &rdst);
    SDL_FreeSurface(lsurf);
    SDL_FreeSurface(rsurf);
    
    CT_Texture *tex = CT_CreateTextureFromSurface(renderer, combined);
    if (!tex) { SDL_FreeSurface(combined); return -1; }
    
    Sprite *s = &atlas->sprites[atlas->count++];
    s->id = new_id;
    s->type = 0x0001; /* composite */
    s->texture = tex;
    s->pixels = combined;
    s->w = w;
    s->h = h;
    
    return 0;
}

int sprites_compose_v(SpriteAtlas *atlas, SDL_Renderer *renderer,
                      uint16_t id_top, uint16_t id_bottom, uint16_t new_id)
{
    Sprite *top = sprites_find(atlas, id_top);
    Sprite *bot = sprites_find(atlas, id_bottom);
    if (!top || !bot || atlas->count >= MAX_SPRITES) return -1;
    
    int w = top->w > bot->w ? top->w : bot->w;
    int h = top->h + bot->h;
    
    SDL_Surface *tsurf = sprite_to_surface(top);
    SDL_Surface *bsurf = sprite_to_surface(bot);
    if (!tsurf || !bsurf) {
        if (tsurf) SDL_FreeSurface(tsurf);
        if (bsurf) SDL_FreeSurface(bsurf);
        return -1;
    }
    
    SDL_Surface *combined = SDL_CreateRGBSurface(0, w, h, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!combined) { SDL_FreeSurface(tsurf); SDL_FreeSurface(bsurf); return -1; }
    SDL_FillRect(combined, NULL, 0);
    SDL_Rect tdst = { 0, 0, top->w, top->h };
    SDL_Rect bdst = { 0, top->h, bot->w, bot->h };
    SDL_BlitSurface(tsurf, NULL, combined, &tdst);
    SDL_BlitSurface(bsurf, NULL, combined, &bdst);
    SDL_FreeSurface(tsurf);
    SDL_FreeSurface(bsurf);
    
    /* Apply white transparency mask (stairs, escalators use white as transparent) */
    SDL_SetColorKey(combined, SDL_TRUE, SDL_MapRGB(combined->format, 0xFF, 0xFF, 0xFF));
    
    CT_Texture *tex = CT_CreateTextureFromSurface(renderer, combined);
    if (!tex) { SDL_FreeSurface(combined); return -1; }
    
    Sprite *s = &atlas->sprites[atlas->count++];
    s->id = new_id;
    s->type = 0x0001; /* composite */
    s->texture = tex;
    s->pixels = combined;
    s->w = w;
    s->h = h;
    
    return 0;
}

void sprites_free(SpriteAtlas *atlas)
{
    for (int i = 0; i < atlas->count; i++) {
        SDL_FreeSurface(atlas->sprites[i].pixels);
        if (atlas->sprites[i].texture)
            CT_DestroyTexture(atlas->sprites[i].texture);
    }
    memset(atlas, 0, sizeof(*atlas));
}
