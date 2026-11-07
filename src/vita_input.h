/* Portable Vita pointer/event core: hardware access lives in vita_platform.c. */
#ifndef CT_VITA_INPUT_H
#define CT_VITA_INPUT_H
#include <SDL.h>
#include "texture.h"
typedef void (*CT_InputSink)(SDL_Event *);
typedef struct {
    float x,y,hold,nudge_hold;
    int w,h,last_x,last_y,visible,cross,circle,touch,focused;
    unsigned dpad,last_click_ms,last_click_count;
    int click_x,click_y;
    SDL_TouchID front;
    SDL_FingerID finger;
    CT_Texture *cursor;
} CT_VitaInput;
void CT_InputInit(CT_VitaInput *,int,int,SDL_TouchID);
int CT_InputEvent(CT_VitaInput *,const SDL_Event *,CT_InputSink);
void CT_InputTouch(CT_VitaInput *,int,float,float,SDL_FingerID,CT_InputSink);
int CT_InputStep(CT_VitaInput *,float,int,int,int,int,float *,float *,CT_InputSink);
void CT_InputDraw(CT_VitaInput *,SDL_Renderer *);
typedef struct { double accumulator; unsigned phase; } CT_SimClock;
int CT_SimSteps(CT_SimClock *,double dt,int speed);
#endif
