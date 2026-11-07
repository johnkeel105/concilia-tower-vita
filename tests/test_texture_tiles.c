/* Compare bounded/tiled uploads to the same unbounded SDL software draws.
 * cc -Isrc $(sdl2-config --cflags) tests/test_texture_tiles.c src/texture.c
 *    $(sdl2-config --libs) -o /tmp/texture-test
 */
#include "texture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SDL_Surface *surface(int w, int h)
{ return SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888); }
static void pattern(SDL_Surface *s)
{
    for (int y=0; y<s->h; y++) {
        Uint32 *row=(Uint32 *)((Uint8 *)s->pixels+y*s->pitch);
        for (int x=0; x<s->w; x++)
            row[x]=SDL_MapRGBA(s->format,(x*13+y*7)%256,(x*3+y*11)%256,(x+y*17)%256,255);
    }
}
static void compare(SDL_Renderer *a, SDL_Renderer *b, SDL_Surface *as, SDL_Surface *bs,
                    CT_Texture *t, SDL_Texture *ref, const SDL_Rect *src, SDL_Rect dst,
                    const char *name)
{
    SDL_SetRenderDrawColor(a,17,22,31,255); SDL_SetRenderDrawColor(b,17,22,31,255);
    SDL_RenderClear(a); SDL_RenderClear(b);
    assert(CT_RenderCopy(a,t,src,&dst)==0);
    assert(SDL_RenderCopy(b,ref,src,&dst)==0);
    SDL_RenderPresent(a); SDL_RenderPresent(b);
    for (int y=0;y<as->h;y++) for (int x=0;x<as->w;x++) {
        Uint32 ap=((Uint32 *)((Uint8 *)as->pixels+y*as->pitch))[x];
        Uint32 bp=((Uint32 *)((Uint8 *)bs->pixels+y*bs->pitch))[x];
        if (ap!=bp) { fprintf(stderr,"%s mismatch x=%d y=%d tiled=%08x ref=%08x\n",name,x,y,ap,bp); assert(ap==bp); }
    }
}
static void run(int limit, int width, int height, int key)
{
    SDL_Surface *src=surface(width,height),*a=surface(200,160),*b=surface(200,160);
    assert(src && a && b); pattern(src);
    if(key==2) {
        for(int y=0;y<src->h;y++) {
            Uint32 *row=(Uint32 *)((Uint8 *)src->pixels+y*src->pitch);
            for(int x=0;x<src->w;x++) row[x]=(row[x]&0xffffffu)|((Uint32)((x*9+y*7)%256)<<24);
        }
        SDL_SetSurfaceColorMod(src,213,197,231); SDL_SetSurfaceAlphaMod(src,187);
    }
    if(key==1) {
        for(int y=0;y<src->h;y++) ((Uint32 *)((Uint8 *)src->pixels+y*src->pitch))[limit-1]=0xffffffffu;
        SDL_SetColorKey(src,SDL_TRUE,0xffffffffu);
    }
    SDL_Renderer *ra=SDL_CreateSoftwareRenderer(a),*rb=SDL_CreateSoftwareRenderer(b);
    assert(ra && rb);
    CT_TextureTestLimit(limit,limit);
    CT_Texture *t=CT_CreateTextureFromSurface(ra,src);
    SDL_Texture *ref=SDL_CreateTextureFromSurface(rb,src);
    assert(t && ref && !CT_TextureIsPlaceholder(t) && CT_TextureTileCount(t)==((width+limit-1)/limit)*((height+limit-1)/limit));
    int sx=width>limit?limit-11:3,sy=height>limit?limit-9:3;
    SDL_Rect crop={sx,sy,26,24};
    compare(ra,rb,a,b,t,ref,&crop,(SDL_Rect){7,9,26,24},"boundary 1:1");
    compare(ra,rb,a,b,t,ref,&crop,(SDL_Rect){7,9,52,48},"boundary 2x");
    compare(ra,rb,a,b,t,ref,&crop,(SDL_Rect){-5,-3,52,48},"screen clipped");
    compare(ra,rb,a,b,t,ref,&crop,(SDL_Rect){7,9,13,12},"boundary half scale");
    compare(ra,rb,a,b,t,ref,&crop,(SDL_Rect){7,9,39,36},"boundary 1.5x");
    compare(ra,rb,a,b,t,ref,&crop,(SDL_Rect){7,9,41,37},"boundary arbitrary scale");
    compare(ra,rb,a,b,t,ref,&crop,(SDL_Rect){-7,-4,41,37},"fractional scale screen clipped");
    if(width>limit && height==36)
        compare(ra,rb,a,b,t,ref,NULL,(SDL_Rect){7,9,159,31},"whole strip scaled");
    SDL_Rect outside={width-18,2,40,24};
    compare(ra,rb,a,b,t,ref,&outside,(SDL_Rect){7,9,40,24},"source clamped to image");
    CT_SetTextureAlphaMod(t,137); SDL_SetTextureAlphaMod(ref,137);
    CT_SetTextureColorMod(t,97,183,224); SDL_SetTextureColorMod(ref,97,183,224);
    CT_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND); SDL_SetTextureBlendMode(ref,SDL_BLENDMODE_BLEND);
    compare(ra,rb,a,b,t,ref,&crop,(SDL_Rect){7,9,26,24},"mods and blend");
    compare(ra,rb,a,b,t,ref,&crop,(SDL_Rect){7,9,41,37},"mods and fractional scale");
    CT_SetTextureScaleMode(t,SDL_ScaleModeNearest);
    SDL_DestroyTexture(ref); CT_DestroyTexture(t);
    CT_TextureTestFailNextUpload(); t=CT_CreateTextureFromSurface(ra,src);
    assert(t && CT_TextureIsPlaceholder(t));
    assert(CT_RenderCopy(ra,t,&crop,&(SDL_Rect){0,0,26,24})==0);
    CT_DestroyTextures(ra);
    SDL_DestroyRenderer(ra); SDL_DestroyRenderer(rb);
    SDL_FreeSurface(src); SDL_FreeSurface(a); SDL_FreeSurface(b);
    printf("PASS limit=%d logical=%dx%d key=%d\n",limit,width,height,key);
}
int main(void)
{
    assert(SDL_Init(0)==0);
    for(int limit=4096;limit>=1024;limit/=4) {
        run(limit,5888,36,0); run(limit,4416,36,1);
        run(limit,5888,36,2); run(limit,36,limit+40,0); run(limit,limit+40,limit+40,1);
    }
    CT_TextureTestLimit(0,0); SDL_Quit();
    puts("PASS: tiled uploads, both axes, crossed boundaries, scale, clipping, mods, color keys, placeholder and cleanup");
}
