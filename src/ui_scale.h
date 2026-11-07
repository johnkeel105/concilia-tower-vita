#ifndef CT_UI_SCALE_H
#define CT_UI_SCALE_H
#include "texture.h"
typedef struct { SDL_Rect src,dst; } CT_UIPanel;
typedef struct { CT_Texture *canvas; CT_UIPanel panel; SDL_BlendMode blend; Uint8 r,g,b,a; } CT_UIScale;
CT_UIPanel CT_UIPanelGeometry(SDL_Rect src,float scale);
int CT_UIPanelHit(CT_UIPanel p,int x,int y);
void CT_UIPanelPoint(CT_UIPanel p,int *x,int *y);
void CT_UIPanelClamp(int *x,int *y,int w,int h,float scale,int top);
int CT_UIScaleBegin(CT_UIScale *,SDL_Renderer *,CT_UIPanel);
int CT_UIScaleEnd(CT_UIScale *,SDL_Renderer *);
#endif
