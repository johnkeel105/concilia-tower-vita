#ifndef CT_WORLD_VIEW_H
#define CT_WORLD_VIEW_H
#include "texture.h"
typedef struct { int screen_w,screen_h,world_w,world_h; float zoom; SDL_Rect dst; CT_Texture *target; } CT_WorldView;
void CT_WorldViewInit(CT_WorldView *,int,int,float);
int CT_WorldViewCreate(CT_WorldView *,SDL_Renderer *);
int CT_WorldViewBegin(CT_WorldView *,SDL_Renderer *);
int CT_WorldViewEnd(CT_WorldView *,SDL_Renderer *);
void CT_ViewSourcePoint(const CT_WorldView *,int,int,int *,int *);
void CT_ViewWorldPoint(const CT_WorldView *,int,int,int,int,int *,int *);
void CT_ViewScreenPoint(const CT_WorldView *,int,int,int,int,int *,int *);
float CT_ViewHalfWidth(const CT_WorldView *);
float CT_ViewHalfHeight(const CT_WorldView *);
void CT_ViewSnapStep(const CT_WorldView *,int,int,int,int,int,int,int,int *,int *);
#endif
