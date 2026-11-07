#include "ui_scale.h"
#include <math.h>
CT_UIPanel CT_UIPanelGeometry(SDL_Rect src,float scale)
{ return (CT_UIPanel){src,{src.x,src.y,(int)lroundf(src.w*scale),(int)lroundf(src.h*scale)}}; }
int CT_UIPanelHit(CT_UIPanel p,int x,int y)
{ return x>=p.dst.x && x<p.dst.x+p.dst.w && y>=p.dst.y && y<p.dst.y+p.dst.h; }
void CT_UIPanelPoint(CT_UIPanel p,int *x,int *y)
{
    *x=p.src.x+(int)floor(((double)*x+0.5-p.dst.x)*p.src.w/p.dst.w);
    *y=p.src.y+(int)floor(((double)*y+0.5-p.dst.y)*p.src.h/p.dst.h);
}
void CT_UIPanelClamp(int *x,int *y,int w,int h,float scale,int top)
{
    int maxx=960-(int)lroundf(w*scale),maxy=544-(int)lroundf(h*scale);
    *x=SDL_max(0,SDL_min(*x,maxx));*y=SDL_max(top,SDL_min(*y,maxy));
}
int CT_UIScaleBegin(CT_UIScale *s,SDL_Renderer *r,CT_UIPanel p)
{
    if(!s->canvas) {
        s->canvas=CT_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,960,544);
        if(!s->canvas) return -1;
        CT_SetTextureBlendMode(s->canvas,SDL_BLENDMODE_BLEND);
        CT_SetTextureScaleMode(s->canvas,SDL_ScaleModeNearest);
    }
    if(CT_SetRenderTarget(r,s->canvas)<0) return -1;
    s->panel=p;SDL_GetRenderDrawBlendMode(r,&s->blend);SDL_GetRenderDrawColor(r,&s->r,&s->g,&s->b,&s->a);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);SDL_SetRenderDrawColor(r,0,0,0,0);SDL_RenderClear(r);
    return 0;
}
int CT_UIScaleEnd(CT_UIScale *s,SDL_Renderer *r)
{
    if(CT_SetRenderTarget(r,NULL)<0) return -1;
    SDL_SetRenderDrawBlendMode(r,s->blend);SDL_SetRenderDrawColor(r,s->r,s->g,s->b,s->a);
    return CT_RenderCopy(r,s->canvas,&s->panel.src,&s->panel.dst);
}
