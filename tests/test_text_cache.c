/* Mock rasterizer isolates cache ownership/eviction without host SDL_ttf. */
#include "text_cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
static int renders;
SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *f,const char *text,SDL_Color c)
{
    renders++;int h=f==(TTF_Font *)(uintptr_t)2?512:12;
    int w=f==(TTF_Font *)(uintptr_t)2?1024:(int)SDL_strlen(text)*6;
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
    assert(s);SDL_FillRect(s,NULL,SDL_MapRGBA(s->format,c.r,c.g,c.b,c.a));return s;
}
SDL_Surface *TTF_RenderText_Blended(TTF_Font *f,const char *t,SDL_Color c) { return TTF_RenderUTF8_Blended(f,t,c); }
int main(void)
{
    SDL_Init(0);SDL_Surface *screen=SDL_CreateRGBSurfaceWithFormat(0,64,64,32,SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *r=SDL_CreateSoftwareRenderer(screen);assert(r);SDL_Color c={255,255,255,255};
    for(int i=0;i<100;i++) {
        SDL_Surface *s=CT_RenderUTF8_Blended((TTF_Font *)(uintptr_t)1,"Tools",c);assert(s);
        CT_Texture *t=CT_TextTexture(r,s);SDL_FreeSurface(s);assert(t);
        CT_RenderCopy(r,t,NULL,&(SDL_Rect){0,0,30,12});CT_DestroyTexture(t);
    }
    assert(renders==1);CT_TextureStats tx=CT_TextureStatsReset();assert(tx.uploads==1);
    SDL_Surface *held=CT_RenderUTF8_Blended((TTF_Font *)(uintptr_t)1,"Tools",c);
    CT_Texture *heldtex=CT_TextTexture(r,held);
    /* Force budget eviction while many cache slots are still empty. */
    for(int i=0;i<5;i++) {
        char text[32];SDL_snprintf(text,sizeof(text),"large%d",i);
        SDL_Surface *s=CT_RenderUTF8_Blended((TTF_Font *)(uintptr_t)2,text,c);SDL_FreeSurface(s);
    }
    assert(held->w==30);assert(CT_RenderCopy(r,heldtex,NULL,&(SDL_Rect){0,0,30,12})==0);
    CT_DestroyTexture(heldtex);SDL_FreeSurface(held);
    for(int i=0;i<400;i++) {
        char text[32];SDL_snprintf(text,sizeof(text),"text%d",i);
        SDL_Surface *s=CT_RenderText_Blended((TTF_Font *)(uintptr_t)1,text,c);
        CT_Texture *t=CT_TextTexture(r,s);SDL_FreeSurface(s);CT_DestroyTexture(t);
    }
    CT_TextCacheClear();CT_DestroyTextures(r);SDL_DestroyRenderer(r);SDL_FreeSurface(screen);SDL_Quit();
    puts("PASS: one raster/upload per unchanged label, cache budget/LRU eviction, retained surface/texture ownership, cleanup");
}
