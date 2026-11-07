/* Logical SDL textures, tiled to the actual renderer limits. */
#ifndef CT_TEXTURE_H
#define CT_TEXTURE_H
#include <SDL.h>
typedef struct CT_Texture CT_Texture;
CT_Texture *CT_CreateTextureFromSurface(SDL_Renderer *, SDL_Surface *);
CT_Texture *CT_CreateTexture(SDL_Renderer *, Uint32, int, int, int);
void CT_DestroyTexture(CT_Texture *);
CT_Texture *CT_RetainTexture(CT_Texture *);
typedef struct { unsigned uploads, tile_draws, scaled_cache_misses; } CT_TextureStats;
CT_TextureStats CT_TextureStatsReset(void);
void CT_DestroyTextures(SDL_Renderer *);
int CT_RenderCopy(SDL_Renderer *, CT_Texture *, const SDL_Rect *, const SDL_Rect *);
int CT_SetTextureAlphaMod(CT_Texture *, Uint8);
int CT_SetTextureColorMod(CT_Texture *, Uint8, Uint8, Uint8);
int CT_SetTextureBlendMode(CT_Texture *, SDL_BlendMode);
int CT_SetTextureScaleMode(CT_Texture *, SDL_ScaleMode);
int CT_SetRenderTarget(SDL_Renderer *, CT_Texture *);
int CT_TextureIsPlaceholder(const CT_Texture *);
int CT_TextureTileCount(const CT_Texture *);
/* Test override only lowers the renderer's advertised limit; 0 restores it. */
void CT_TextureTestLimit(int width, int height);
/* Test fault injection: fail one upload, then restore normal behavior. */
void CT_TextureTestFailNextUpload(void);
#endif
