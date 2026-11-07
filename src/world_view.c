#include "world_view.h"
#include <math.h>
#include <string.h>
void CT_WorldViewInit(CT_WorldView *v,int w,int h,float z)
{
    memset(v,0,sizeof(*v));v->screen_w=w;v->screen_h=h;v->zoom=z;
    v->world_w=(int)ceilf(w/z);v->world_h=(int)ceilf(h/z);
    /* Even source dimensions keep camera center exact with 3:2 scaling.
     * 960x544 -> 640x364 -> 960x546, crop one output row at each edge. */
    if(v->world_w&1) v->world_w++;
    if(v->world_h&1) v->world_h++;
    v->dst.w=(int)lroundf(v->world_w*z);v->dst.h=(int)lroundf(v->world_h*z);
    v->dst.x=(w-v->dst.w)/2;v->dst.y=(h-v->dst.h)/2;
}
int CT_WorldViewCreate(CT_WorldView *v,SDL_Renderer *r)
{
    v->target=CT_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,v->world_w,v->world_h);
    if(!v->target) return -1;
    CT_SetTextureScaleMode(v->target,SDL_ScaleModeNearest);
    CT_SetTextureBlendMode(v->target,SDL_BLENDMODE_NONE);
    if(CT_SetRenderTarget(r,v->target)<0) {
        CT_DestroyTexture(v->target);v->target=NULL;return -1;
    }
    if(CT_SetRenderTarget(r,NULL)<0) { CT_DestroyTexture(v->target);v->target=NULL;return -1; }
    return 0;
}
int CT_WorldViewBegin(CT_WorldView *v,SDL_Renderer *r) { return CT_SetRenderTarget(r,v->target); }
int CT_WorldViewEnd(CT_WorldView *v,SDL_Renderer *r)
{
    if(CT_SetRenderTarget(r,NULL)<0) return -1;
    return CT_RenderCopy(r,v->target,NULL,&v->dst);
}
void CT_ViewSourcePoint(const CT_WorldView *v,int x,int y,int *sx,int *sy)
{
    /* Match nearest-neighbor destination pixel centers, not a truncating
     * divide that can select the wrong cell on half-pixel grid boundaries. */
    *sx=(int)floor(((double)x+0.5-v->dst.x)*v->world_w/v->dst.w);
    *sy=(int)floor(((double)y+0.5-v->dst.y)*v->world_h/v->dst.h);
}
void CT_ViewWorldPoint(const CT_WorldView *v,int x,int y,int cx,int cy,int *wx,int *wy)
{ CT_ViewSourcePoint(v,x,y,wx,wy);*wx+=cx-v->world_w/2;*wy+=cy-v->world_h/2; }
void CT_ViewScreenPoint(const CT_WorldView *v,int wx,int wy,int cx,int cy,int *x,int *y)
{
    *x=(int)ceil((double)(wx-cx+v->world_w/2)*v->dst.w/v->world_w+v->dst.x-0.5);
    *y=(int)ceil((double)(wy-cy+v->world_h/2)*v->dst.h/v->world_h+v->dst.y-0.5);
}
float CT_ViewHalfWidth(const CT_WorldView *v) { return v->screen_w/(2*v->zoom); }
float CT_ViewHalfHeight(const CT_WorldView *v) { return v->screen_h/(2*v->zoom); }

void CT_ViewSnapStep(const CT_WorldView *v,int x,int y,int cx,int cy,int dir,int cw,int ch,int *wx,int *wy)
{
    CT_ViewWorldPoint(v,x,y,cx,cy,wx,wy);
    int gx=(int)floor((double)*wx/cw),gy=(int)floor((double)*wy/ch);
    gx+=(dir==1)-(dir==0);gy+=(dir==3)-(dir==2);
    *wx=gx*cw+cw/2;*wy=gy*ch+ch/2;
}
