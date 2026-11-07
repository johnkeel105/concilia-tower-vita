#include "vita_input.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static SDL_Event events[4096];static int count;
static void collect(SDL_Event *e) { assert(count<4096);events[count++]=*e; }
static void button(CT_VitaInput *p,int b,int down)
{ SDL_Event e;SDL_zero(e);e.type=down?SDL_CONTROLLERBUTTONDOWN:SDL_CONTROLLERBUTTONUP;e.cbutton.button=b;assert(CT_InputEvent(p,&e,collect)); }
int main(void)
{
    SDL_Init(0);CT_VitaInput p;CT_InputInit(&p,960,544,1);float cx=0,cy=0;
    CT_InputStep(&p,1.f/60,1000,-2000,7999,-7999,&cx,&cy,collect);
    assert(cx==0 && cy==0 && p.x==480 && p.y==272);
    CT_InputStep(&p,1.f/60,32767,0,0,0,&cx,&cy,collect);
    assert(cx>0 && p.x==480);float oldcx=cx;
    CT_InputStep(&p,1.f/60,0,0,32767,0,&cx,&cy,collect);assert(p.x>480 && cx==oldcx);
    button(&p,SDL_CONTROLLER_BUTTON_A,1);assert(events[count-1].type==SDL_MOUSEBUTTONDOWN);
    CT_InputStep(&p,1.f/60,0,0,32767,0,&cx,&cy,collect);
    assert(events[count-1].type==SDL_MOUSEMOTION && (events[count-1].motion.state&SDL_BUTTON_LMASK));
    button(&p,SDL_CONTROLLER_BUTTON_A,0);assert(events[count-1].type==SDL_MOUSEBUTTONUP);
    button(&p,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,1);assert(events[count-1].button.button==SDL_BUTTON_RIGHT);
    button(&p,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,0);
    button(&p,SDL_CONTROLLER_BUTTON_B,1);assert(events[count-1].key.keysym.sym==SDLK_F13);
    button(&p,SDL_CONTROLLER_BUTTON_Y,1);assert(events[count-1].key.keysym.sym==SDLK_F14);
    button(&p,SDL_CONTROLLER_BUTTON_X,1);assert(events[count-1].key.keysym.sym==SDLK_F15);
    button(&p,SDL_CONTROLLER_BUTTON_A,1);int held_count=count;
    button(&p,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);assert(count==held_count);
    button(&p,SDL_CONTROLLER_BUTTON_A,0);assert(count==held_count && p.rtrigger);
    CT_InputStep(&p,1.f/60,0,0,32767,0,&cx,&cy,collect);
    assert(events[count-1].motion.state&SDL_BUTTON_LMASK);
    button(&p,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);assert(events[count-1].type==SDL_MOUSEBUTTONUP);
    p.ui_nav=1;oldcx=p.x;button(&p,SDL_CONTROLLER_BUTTON_DPAD_LEFT,1);assert(p.x==oldcx && events[count-1].key.keysym.sym==SDLK_F16);
    int repeats=count;for(int i=0;i<60;i++) CT_InputStep(&p,1.f/60,0,0,0,0,&cx,&cy,collect);assert(count>repeats && p.x==oldcx);
    button(&p,SDL_CONTROLLER_BUTTON_DPAD_LEFT,0);button(&p,SDL_CONTROLLER_BUTTON_A,1);assert(events[count-1].type==SDL_KEYDOWN && events[count-1].key.keysym.sym==SDLK_RETURN && !p.cross);
    p.ui_nav=0;button(&p,SDL_CONTROLLER_BUTTON_A,0);assert(events[count-1].type==SDL_KEYUP && !p.confirm_cross);
    p.ui_nav=2;button(&p,SDL_CONTROLLER_BUTTON_A,1);assert(events[count-1].type==SDL_MOUSEBUTTONDOWN && p.cross);button(&p,SDL_CONTROLLER_BUTTON_A,0);assert(events[count-1].type==SDL_MOUSEBUTTONUP);
    p.ui_nav=0;
    oldcx=p.x;button(&p,SDL_CONTROLLER_BUTTON_DPAD_LEFT,1);assert(p.x==oldcx-2);
    button(&p,SDL_CONTROLLER_BUTTON_DPAD_LEFT,0);
    button(&p,SDL_CONTROLLER_BUTTON_START,1);assert(events[count-1].key.keysym.sym==SDLK_F10);
    int n=count;SDL_Event e;SDL_zero(e);e.type=SDL_FINGERDOWN;e.tfinger.touchId=2;
    e.tfinger.x=.1f;e.tfinger.y=.1f;CT_InputEvent(&p,&e,collect);assert(count==n);
    e.tfinger.touchId=1;e.tfinger.fingerId=9;e.tfinger.x=.5f;e.tfinger.y=.5f;
    CT_InputEvent(&p,&e,collect);assert(p.touch && p.x==480 && p.y==272);
    assert(events[count-1].type==SDL_MOUSEBUTTONDOWN);
    e.type=SDL_FINGERMOTION;e.tfinger.x=1;e.tfinger.y=1;CT_InputEvent(&p,&e,collect);
    assert(p.x==959 && p.y==543 && events[count-1].motion.state&SDL_BUTTON_LMASK);
    e.type=SDL_FINGERUP;CT_InputEvent(&p,&e,collect);assert(!p.touch && events[count-1].type==SDL_MOUSEBUTTONUP);
    /* Touch plus Cross has only one aggregated left-down and left-up edge. */
    button(&p,SDL_CONTROLLER_BUTTON_A,1);
    CT_InputTouch(&p,SDL_FINGERDOWN,.25f,.25f,4,collect);
    n=count;button(&p,SDL_CONTROLLER_BUTTON_A,0);assert(count==n && p.touch);
    CT_InputTouch(&p,SDL_FINGERUP,.25f,.25f,4,collect);assert(events[count-1].type==SDL_MOUSEBUTTONUP);
    e.type=SDL_MOUSEBUTTONDOWN;e.button.which=SDL_TOUCH_MOUSEID;n=count;
    assert(CT_InputEvent(&p,&e,collect)==1 && count==n);
    button(&p,SDL_CONTROLLER_BUTTON_A,1);
    SDL_zero(e);e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    CT_InputEvent(&p,&e,collect);assert(!p.cross && !p.touch && events[count-1].type==SDL_MOUSEBUTTONUP);
    for(int fps=30;fps<=120;fps*=2) {
        CT_SimClock c={0};int ticks=0;for(int i=0;i<fps*10;i++) ticks+=CT_SimSteps(&c,1.0/fps,1);
        assert(ticks==150);
        c=(CT_SimClock){0};ticks=0;for(int i=0;i<fps*10;i++) ticks+=CT_SimSteps(&c,1.0/fps,2);
        assert(ticks==600);
    }
    CT_SimClock c={0};assert(CT_SimSteps(&c,100,3)<=12);
    SDL_Surface *screen=SDL_CreateRGBSurfaceWithFormat(0,960,544,32,SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *r=SDL_CreateSoftwareRenderer(screen);assert(r);CT_InputDraw(&p,r);
    SDL_RenderPresent(r);assert(p.cursor && !CT_TextureIsPlaceholder(p.cursor));
    CT_DestroyTextures(r);SDL_DestroyRenderer(r);SDL_FreeSurface(screen);SDL_Quit();
    puts("PASS: latest stick mapping, deadzone, cursor clamp, click/drag, touch, duplicate suppression, focus release, cursor draw, 15/60Hz sim independent of 30/60/120fps");
}
