#include "ui_focus.h"
#include <math.h>
int CT_UIFocusNext(const SDL_Rect *r,int n,int current,int dir)
{
    if(n<=0) return -1;
    if(current<0 || current>=n) return 0;
    int best=-1;double score=1e30;
    double x=r[current].x+r[current].w*.5,y=r[current].y+r[current].h*.5;
    for(int i=0;i<n;i++) if(i!=current) {
        double dx=r[i].x+r[i].w*.5-x,dy=r[i].y+r[i].h*.5-y;
        double forward=dir==0?-dx:dir==1?dx:dir==2?-dy:dy;
        double side=dir<2?fabs(dy):fabs(dx);
        if(forward<=.5) continue;
        double s=forward+side*4+side*side/forward;
        if(s<score) {score=s;best=i;}
    }
    /* Wrap predictably when no candidate exists in that direction. */
    if(best<0) {
        score=1e30;
        for(int i=0;i<n;i++) if(i!=current) {
            double dx=r[i].x+r[i].w*.5-x,dy=r[i].y+r[i].h*.5-y;
            double forward=dir==0?-dx:dir==1?dx:dir==2?-dy:dy;
            double side=dir<2?fabs(dy):fabs(dx),s=forward+side*10000;
            if(s<score) { score=s;best=i; }
        }
    }
    return best<0?current:best;
}
