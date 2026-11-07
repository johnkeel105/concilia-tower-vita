#include "cursor.h"
#include <string.h>
static unsigned u16(const unsigned char *b) { return b[0]|((unsigned)b[1]<<8); }
static unsigned u32(const unsigned char *b) { return u16(b)|((unsigned)u16(b+2)<<16); }
void CT_CursorImageFree(CT_CursorImage *c)
{ SDL_FreeSurface(c->color);SDL_FreeSurface(c->invert);memset(c,0,sizeof(*c)); }
int CT_CursorDecode(const unsigned char *b,size_t n,CT_CursorImage *c)
{
    memset(c,0,sizeof(*c));
    if(!b || n<52 || u32(b+4)!=40 || u16(b+16)!=1 || u16(b+18)!=1 || u32(b+20)!=0) return -1;
    unsigned w=u32(b+8),hh=u32(b+12),hx=u16(b),hy=u16(b+2);
    if(!w || w>128 || hh<2 || hh>256 || (hh&1) || hx>=w || hy>=hh/2) return -1;
    unsigned h=hh/2,stride=((w+31)/32)*4;size_t bytes=(size_t)stride*h;
    if(n<52+2*bytes) return -1;
    c->color=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
    c->invert=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
    if(!c->color || !c->invert) { CT_CursorImageFree(c);return -1; }
    SDL_FillRect(c->color,NULL,0);SDL_FillRect(c->invert,NULL,0);c->hot_x=hx;c->hot_y=hy;
    for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++) {
        unsigned k=(h-1-y)*stride+x/8,bit=128>>(x%8);
        int v=!!(b[52+k]&bit),a=!!(b[52+bytes+k]&bit);
        Uint32 *color=(Uint32*)((Uint8*)c->color->pixels+y*c->color->pitch);
        Uint32 *inv=(Uint32*)((Uint8*)c->invert->pixels+y*c->invert->pitch);
        if(a && v) { inv[x]=0xffffffff;c->inverted++; }
        else if(!a) { const unsigned char *rgb=b+44+4*v;color[x]=0xff000000u|((Uint32)rgb[2]<<16)|((Uint32)rgb[1]<<8)|rgb[0]; }
    }
    return 0;
}
int CT_CursorsLoad(CT_CursorBank *bank,SDL_Renderer *r,NEResourceTable *exe)
{
    int loaded=0;
    for(int i=1;i<CT_CURSOR_COUNT;i++) {
        NEResource *group=ne_find(exe,0x800c,(uint16_t)(0x8000+1001+i));
        if(!group || group->length<20 || u16(group->data+2)!=2 || u16(group->data+4)<1) continue;
        unsigned id=u16(group->data+18);NEResource *res=ne_find(exe,0x8001,(uint16_t)(0x8000|id));
        CT_CursorImage image;if(!res || res->length<52 || CT_CursorDecode(res->data,res->length,&image)<0) continue;
        CT_Cursor *c=&bank->cursors[i];c->w=image.color->w;c->h=image.color->h;c->x=image.hot_x;c->y=image.hot_y;
        c->color=CT_CreateTextureFromSurface(r,image.color);c->ready=c->color && !CT_TextureIsPlaceholder(c->color);
        if(c->color) { CT_SetTextureBlendMode(c->color,SDL_BLENDMODE_BLEND);CT_SetTextureScaleMode(c->color,SDL_ScaleModeNearest); }
        if(image.inverted && c->ready) {
            c->invert=CT_CreateTextureFromSurface(r,image.invert);
            SDL_BlendMode mode=SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR,SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,SDL_BLENDOPERATION_ADD,SDL_BLENDFACTOR_ZERO,SDL_BLENDFACTOR_ONE,SDL_BLENDOPERATION_ADD);
            if(!c->invert || CT_TextureIsPlaceholder(c->invert) || CT_SetTextureBlendMode(c->invert,mode)<0) c->ready=0;
            if(c->invert) CT_SetTextureScaleMode(c->invert,SDL_ScaleModeNearest);
        }
        SDL_Log("EXE cursor group=%u image=%u size=%dx%d hotspot=%d,%d invert=%d ready=%d",1001+i,id,c->w,c->h,c->x,c->y,image.inverted,c->ready);
        loaded+=c->ready;CT_CursorImageFree(&image);
    }
    return loaded;
}
void CT_CursorsDraw(CT_CursorBank *bank,CT_VitaInput *p,SDL_Renderer *r,int kind)
{
    if(!p->visible) return;
    if(kind<1 || kind>=CT_CURSOR_COUNT || !bank->cursors[kind].ready) { CT_InputDraw(p,r);return; }
    CT_Cursor *c=&bank->cursors[kind];SDL_Rect dst={(int)p->x-c->x,(int)p->y-c->y,c->w,c->h};
    SDL_Rect clip;SDL_bool enabled=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&clip);SDL_RenderSetClipRect(r,NULL);
    CT_RenderCopy(r,c->color,NULL,&dst);if(c->invert) CT_RenderCopy(r,c->invert,NULL,&dst);
    SDL_RenderSetClipRect(r,enabled?&clip:NULL);
}
