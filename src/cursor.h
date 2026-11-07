#ifndef CT_CURSOR_H
#define CT_CURSOR_H
#include "ne_resource.h"
#include "vita_input.h"
enum { CT_CURSOR_ARROW,CT_CURSOR_BUILD,CT_CURSOR_INSPECT,CT_CURSOR_HAND,CT_CURSOR_BULLDOZER,CT_CURSOR_COUNT };
typedef struct { SDL_Surface *color,*invert; int hot_x,hot_y,inverted; } CT_CursorImage;
typedef struct { CT_Texture *color,*invert; int w,h,x,y,ready; } CT_Cursor;
typedef struct { CT_Cursor cursors[CT_CURSOR_COUNT]; } CT_CursorBank;
int CT_CursorDecode(const unsigned char *,size_t,CT_CursorImage *);
void CT_CursorImageFree(CT_CursorImage *);
int CT_CursorsLoad(CT_CursorBank *,SDL_Renderer *,NEResourceTable *);
void CT_CursorsDraw(CT_CursorBank *,CT_VitaInput *,SDL_Renderer *,int);
#endif
