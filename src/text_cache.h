#ifndef CT_TEXT_CACHE_H
#define CT_TEXT_CACHE_H
#include <SDL_ttf.h>
#include "texture.h"
SDL_Surface *CT_RenderUTF8_Blended(TTF_Font *,const char *,SDL_Color);
SDL_Surface *CT_RenderText_Blended(TTF_Font *,const char *,SDL_Color);
CT_Texture *CT_TextTexture(SDL_Renderer *,SDL_Surface *);
void CT_TextCacheClear(void);
typedef struct { unsigned hits,misses; } CT_TextStats;
CT_TextStats CT_TextStatsReset(void);
#endif
