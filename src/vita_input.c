#include "vita_input.h"
#include <math.h>
#include <string.h>
#define POINTER_ID 0x4354564du
static Uint32 buttons(const CT_VitaInput *p)
{ return ((p->cross||p->rtrigger||p->touch)?SDL_BUTTON_LMASK:0)|(p->secondary?SDL_BUTTON_RMASK:0); }
static void motion(CT_VitaInput *p,CT_InputSink sink)
{
    int x=(int)p->x,y=(int)p->y;
    if(x==p->last_x && y==p->last_y) return;
    SDL_Event e; SDL_zero(e); e.type=SDL_MOUSEMOTION;
    e.motion.which=POINTER_ID;e.motion.x=x;e.motion.y=y;
    e.motion.xrel=p->last_x<0?0:x-p->last_x;e.motion.yrel=p->last_y<0?0:y-p->last_y;
    e.motion.state=buttons(p);p->last_x=x;p->last_y=y;sink(&e);
}
static void edge(CT_VitaInput *p,int button,int was,int now,CT_InputSink sink)
{
    if(was==now) return;
    SDL_Event e;SDL_zero(e);e.type=now?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;
    e.button.which=POINTER_ID;e.button.button=button;e.button.state=now?SDL_PRESSED:SDL_RELEASED;
    if(button==SDL_BUTTON_LEFT && now) {
        Uint32 time=SDL_GetTicks();
        int same=p->last_click_count && time-p->last_click_ms<=400 &&
            fabsf(p->x-p->click_x)<=4 && fabsf(p->y-p->click_y)<=4;
        p->last_click_count=same && p->last_click_count==1?2:1;
        p->last_click_ms=time;p->click_x=(int)p->x;p->click_y=(int)p->y;
    }
    e.button.clicks=button==SDL_BUTTON_LEFT?p->last_click_count:1;e.button.x=(int)p->x;e.button.y=(int)p->y;sink(&e);
}
static void clamp(CT_VitaInput *p)
{ p->x=SDL_max(0,SDL_min(p->x,p->w-1));p->y=SDL_max(0,SDL_min(p->y,p->h-1)); }
void CT_InputInit(CT_VitaInput *p,int w,int h,SDL_TouchID front)
{
    memset(p,0,sizeof(*p));p->w=w;p->h=h;p->x=w/2;p->y=h/2;
    p->last_x=p->last_y=-1;p->front=front;p->visible=p->focused=1;
}
void CT_InputTouch(CT_VitaInput *p,int type,float x,float y,SDL_FingerID id,CT_InputSink sink)
{
    if(!p->focused || !isfinite(x) || !isfinite(y)) return;
    if(type==SDL_FINGERDOWN) { if(p->touch) return; p->finger=id; }
    else if(!p->touch || p->finger!=id) return;
    int was=p->cross||p->rtrigger||p->touch;
    p->x=x*p->w;p->y=y*p->h;clamp(p);p->visible=1;motion(p,sink);
    if(type==SDL_FINGERDOWN) p->touch=1;
    else if(type==SDL_FINGERUP) p->touch=0;
    edge(p,SDL_BUTTON_LEFT,was,p->cross||p->rtrigger||p->touch,sink);
}
int CT_InputEvent(CT_VitaInput *p,const SDL_Event *ev,CT_InputSink sink)
{
    if(ev->type==SDL_FINGERDOWN || ev->type==SDL_FINGERMOTION || ev->type==SDL_FINGERUP) {
        if(ev->tfinger.touchId==p->front) CT_InputTouch(p,ev->type,ev->tfinger.x,ev->tfinger.y,ev->tfinger.fingerId,sink);
        return 1;
    }
    if(ev->type==SDL_MOUSEMOTION || ev->type==SDL_MOUSEBUTTONDOWN || ev->type==SDL_MOUSEBUTTONUP) {
        Uint32 id=ev->type==SDL_MOUSEMOTION?ev->motion.which:ev->button.which;
        if(id==SDL_TOUCH_MOUSEID) return 1; /* Manual finger translation owns touch. */
        p->x=ev->type==SDL_MOUSEMOTION?ev->motion.x:ev->button.x;
        p->y=ev->type==SDL_MOUSEMOTION?ev->motion.y:ev->button.y;
        clamp(p);p->visible=1;p->last_x=(int)p->x;p->last_y=(int)p->y;
        return 0;
    }
    if(ev->type==SDL_CONTROLLERDEVICEREMOVED) {
        int left=p->cross||p->rtrigger||p->touch,right=p->secondary;
        p->cross=p->rtrigger=p->secondary=0;p->dpad=0;
        edge(p,SDL_BUTTON_LEFT,left,p->touch,sink);edge(p,SDL_BUTTON_RIGHT,right,0,sink);
    }
    if(ev->type==SDL_WINDOWEVENT && ev->window.event==SDL_WINDOWEVENT_FOCUS_LOST) {
        int left=p->cross||p->rtrigger||p->touch,right=p->secondary;
        p->cross=p->rtrigger=p->secondary=p->touch=0;p->dpad=0;p->focused=0;p->hold=0;
        edge(p,SDL_BUTTON_LEFT,left,0,sink);edge(p,SDL_BUTTON_RIGHT,right,0,sink);
    }
    if(ev->type==SDL_WINDOWEVENT && ev->window.event==SDL_WINDOWEVENT_FOCUS_GAINED) p->focused=1;
    if(ev->type!=SDL_CONTROLLERBUTTONDOWN && ev->type!=SDL_CONTROLLERBUTTONUP) return 0;
    int down=ev->type==SDL_CONTROLLERBUTTONDOWN,b=ev->cbutton.button;
    if(!p->focused) return 1;
    p->visible=1;motion(p,sink);
    if(b==SDL_CONTROLLER_BUTTON_A || b==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
        int was=p->cross||p->rtrigger||p->touch;
        if(b==SDL_CONTROLLER_BUTTON_A) p->cross=down;else p->rtrigger=down;edge(p,SDL_BUTTON_LEFT,was,p->cross||p->rtrigger||p->touch,sink);return 1;
    }
    if(b==SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
        int was=p->secondary;p->secondary=down;edge(p,SDL_BUTTON_RIGHT,was,down,sink);return 1;
    }
    int bit=0,dx=0,dy=0;
    switch(b) {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:bit=1;dx=-2;break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:bit=2;dx=2;break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:bit=4;dy=-2;break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:bit=8;dy=2;break;
    }
    if(bit) {
        if(down && !(p->dpad&bit)) { p->x+=dx;p->y+=dy;clamp(p);motion(p,sink);p->nudge_hold=0; }
        if(down) p->dpad|=bit;else p->dpad&=~bit;
        return 1;
    }
    SDL_Keycode key=SDLK_UNKNOWN;
    switch(b) {
    case SDL_CONTROLLER_BUTTON_START:key=SDLK_F10;break;
    case SDL_CONTROLLER_BUTTON_BACK:key=SDLK_BACKQUOTE;break;
    case SDL_CONTROLLER_BUTTON_B:key=SDLK_F13;break; /* Hand */
    case SDL_CONTROLLER_BUTTON_Y:key=SDLK_F14;break; /* Inspect */
    case SDL_CONTROLLER_BUTTON_X:key=SDLK_F15;break; /* Bulldozer */
    }
    if(key!=SDLK_UNKNOWN) {
        SDL_Event e;SDL_zero(e);e.type=down?SDL_KEYDOWN:SDL_KEYUP;
        e.key.state=down?SDL_PRESSED:SDL_RELEASED;e.key.keysym.sym=key;
        e.key.keysym.scancode=SDL_GetScancodeFromKey(key);sink(&e);
    }
    return 1;
}
static float axis(int v)
{
    float a=fabsf((float)v);if(a<=8000) return 0;
    float q=SDL_min((a-8000)/24767.0f,1.0f);return v<0?-q:q;
}
int CT_InputStep(CT_VitaInput *p,float dt,int lx,int ly,int rx,int ry,float *cx,float *cy,CT_InputSink sink)
{
    if(!p->focused) return 0;
    dt=SDL_max(0,SDL_min(dt,0.05f));float x=axis(rx),y=axis(ry);
    if(x || y) {
        p->hold+=dt;float accel=SDL_min(1.0f,0.4f+p->hold*2);
        p->x+=(55*x+600*x*fabsf(x))*dt*accel;
        p->y+=(55*y+600*y*fabsf(y))*dt*accel;p->visible=1;
    } else p->hold=0;
    if(p->dpad) {
        p->nudge_hold+=dt;
        if(p->nudge_hold>0.25f) {
            p->x+=((!!(p->dpad&2))-(!!(p->dpad&1)))*80*dt;
            p->y+=((!!(p->dpad&8))-(!!(p->dpad&4)))*80*dt;
        }
    } else p->nudge_hold=0;
    clamp(p);motion(p,sink);
    x=axis(lx);y=axis(ly);*cx+=x*700*dt;*cy+=y*700*dt;
    return x!=0 || y!=0;
}
void CT_InputDraw(CT_VitaInput *p,SDL_Renderer *r)
{
    if(!p->visible) return;
    if(!p->cursor) {
        SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,16,24,32,SDL_PIXELFORMAT_ARGB8888);
        if(!s) return;
        SDL_FillRect(s,NULL,0);
        for(int y=0;y<19;y++) for(int x=0;x<=y/2 && x<10;x++) {
            Uint32 *row=(Uint32 *)((Uint8 *)s->pixels+y*s->pitch);
            row[x]=(x==0 || x==y/2 || y==18)?0xff000000u:0xffffffffu;
        }
        for(int y=12;y<23;y++) for(int x=4;x<8;x++) {
            Uint32 *row=(Uint32 *)((Uint8 *)s->pixels+y*s->pitch);
            row[x]=(x==4 || x==7 || y==22)?0xff000000u:0xffffffffu;
        }
        p->cursor=CT_CreateTextureFromSurface(r,s);SDL_FreeSurface(s);
        if(p->cursor) CT_SetTextureBlendMode(p->cursor,SDL_BLENDMODE_BLEND);
    }
    SDL_Rect old;SDL_bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&old);
    SDL_RenderSetClipRect(r,NULL);
    SDL_Rect dst={(int)p->x,(int)p->y,16,24};CT_RenderCopy(r,p->cursor,NULL,&dst);
    SDL_RenderSetClipRect(r,clipped?&old:NULL);
}
/* GameSpeed: paused=0, normal=1, fast=2, turbo=3. Paused still gets
 * 15 Hz maintenance calls; game_update itself freezes simulated time. */
int CT_SimSteps(CT_SimClock *c,double dt,int speed)
{
    if(!isfinite(dt)||dt<0) dt=0;
    if(dt>0.1) dt=0.1;
    c->accumulator+=dt*60.0;int base=(int)(c->accumulator+1e-7);
    c->accumulator-=base;
    if(base>4) { base=4;c->accumulator=0; }
    int normal=(c->phase+base)/4;c->phase=(c->phase+base)%4;
    return speed==3?base*3:speed==2?base:normal;
}
