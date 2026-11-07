#include "world_view.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static Uint32 color(int x,int y) {return 0xff000000u|((x%251)<<16)|((y%251)<<8)|((x+y)%251);}
int main(void) {
 SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO)==0);
 SDL_Surface *screen=SDL_CreateRGBSurfaceWithFormat(0,960,544,32,SDL_PIXELFORMAT_ARGB8888);
 SDL_Renderer *r=SDL_CreateSoftwareRenderer(screen);assert(r);assert(SDL_RenderSetLogicalSize(r,960,544)==0);
 CT_WorldView v;CT_WorldViewInit(&v,960,544,1.5f);
 assert(v.world_w==640 && v.world_h==364 && v.dst.w==960 && v.dst.h==546 && v.dst.y==-1);
 assert(CT_ViewHalfWidth(&v)==320 && CT_ViewHalfHeight(&v)>181.3f && CT_ViewHalfHeight(&v)<181.4f);
 assert(CT_WorldViewCreate(&v,r)==0);
 SDL_Surface *source=SDL_CreateRGBSurfaceWithFormat(0,640,364,32,SDL_PIXELFORMAT_ARGB8888);
 for(int y=0;y<364;y++)for(int x=0;x<640;x++) ((Uint32*)((char*)source->pixels+y*source->pitch))[x]=color(x,y);
 /* Force tile splits before composing; world upscale must have no tile seams. */
 CT_TextureTestLimit(127,127);
 CT_Texture *tiles=CT_CreateTextureFromSurface(r,source);assert(tiles && CT_TextureTileCount(tiles)>1);CT_TextureTestLimit(0,0);
 assert(CT_WorldViewBegin(&v,r)==0);assert(CT_RenderCopy(r,tiles,NULL,NULL)==0);
 assert(CT_WorldViewEnd(&v,r)==0);SDL_RenderPresent(r);
 SDL_Surface *ref=SDL_CreateRGBSurfaceWithFormat(0,960,544,32,SDL_PIXELFORMAT_ARGB8888);
 SDL_Renderer *rr=SDL_CreateSoftwareRenderer(ref);assert(rr);
 SDL_Texture *rt=SDL_CreateTextureFromSurface(rr,source);assert(rt);
 SDL_SetTextureScaleMode(rt,SDL_ScaleModeNearest);SDL_SetTextureBlendMode(rt,SDL_BLENDMODE_NONE);
 assert(SDL_RenderCopy(rr,rt,NULL,&v.dst)==0);SDL_RenderPresent(rr);
 int mismatches=0;
 for(int y=0;y<544;y++)for(int x=0;x<960;x++) {
  int sx,sy;CT_ViewSourcePoint(&v,x,y,&sx,&sy);
  Uint32 actual=((Uint32*)((char*)screen->pixels+y*screen->pitch))[x];
  Uint32 expected=((Uint32*)((char*)ref->pixels+y*ref->pitch))[x];
  if(actual!=expected) mismatches++;
  for(int c=-1;c<=1;c++) {int wx,wy,px,py;CT_ViewWorldPoint(&v,x,y,c*111,-c*73,&wx,&wy);
   assert(wx==sx+c*111-320 && wy==sy-c*73-182);
   CT_ViewScreenPoint(&v,wx,wy,c*111,-c*73,&px,&py);
   int ssx,ssy;CT_ViewSourcePoint(&v,px,py,&ssx,&ssy);assert(ssx==sx && ssy==sy);
  }
 }
 printf("world nearest pixel mismatches=%d\n",mismatches);assert(mismatches==0);
 SDL_SetRenderDrawColor(r,255,0,255,255);SDL_Rect ui={20,20,100,30};SDL_RenderFillRect(r,&ui);SDL_RenderPresent(r);
 assert(((Uint32*)((char*)screen->pixels+20*screen->pitch))[20]==0xffff00ff);
 assert(((Uint32*)((char*)screen->pixels+49*screen->pitch))[119]==0xffff00ff);
 assert(((Uint32*)((char*)screen->pixels+50*screen->pitch))[119]!=0xffff00ff);
 SDL_DestroyTexture(rt);SDL_DestroyRenderer(rr);SDL_FreeSurface(ref);
 CT_DestroyTextures(r);SDL_FreeSurface(source);SDL_DestroyRenderer(r);SDL_FreeSurface(screen);SDL_Quit();
 puts("world-view: 1.5x nearest, cropped centering, pixel-accurate input, seam-free composition and unscaled UI OK");
}
