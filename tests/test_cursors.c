#include "cursor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put32(unsigned char *p,unsigned v) {p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
int main(int argc,char **argv) {
    SDL_Init(0);unsigned char b[68]={0};b[0]=1;b[2]=1;put32(b+4,40);put32(b+8,2);put32(b+12,4);b[16]=b[18]=1;b[48]=b[49]=b[50]=255;
    /* Bottom-up: top row transparent/invert; bottom row black/white. */
    b[52]=0x40;b[56]=0x40;b[60]=0;b[64]=0xc0;
    CT_CursorImage c;assert(CT_CursorDecode(b,sizeof(b),&c)==0);assert(c.hot_x==1 && c.hot_y==1 && c.inverted==1);
    Uint32 *top=c.color->pixels,*bottom=(Uint32*)((Uint8*)c.color->pixels+c.color->pitch);
    assert(top[0]==0 && top[1]==0 && bottom[0]==0xff000000 && bottom[1]==0xffffffff);
    assert(((Uint32*)c.invert->pixels)[1]==0xffffffff);CT_CursorImageFree(&c);
    assert(CT_CursorDecode(b,60,&c)<0);b[0]=2;assert(CT_CursorDecode(b,sizeof(b),&c)<0);b[0]=1;put32(b+8,0x7fffffff);assert(CT_CursorDecode(b,sizeof(b),&c)<0);
    SDL_Surface *screen=SDL_CreateRGBSurfaceWithFormat(0,64,64,32,SDL_PIXELFORMAT_ARGB8888);SDL_Renderer *r=SDL_CreateSoftwareRenderer(screen);assert(r);
    CT_VitaInput p;CT_InputInit(&p,64,64,1);p.x=10;p.y=10;SDL_SetRenderDrawColor(r,20,40,60,255);SDL_RenderClear(r);CT_InputDraw(&p,r);SDL_RenderPresent(r);
    Uint32 *row=(Uint32*)((Uint8*)screen->pixels+10*screen->pitch);assert(row[10]==0xff000000);assert(row[11]==0xff14283c);
    /* The fallback stem is diagonal, with transparent surroundings. */
    row=(Uint32*)((Uint8*)screen->pixels+24*screen->pitch);assert(row[15]==0xff000000 && row[16]==0xffffffff && row[17]==0xffffffff && row[18]==0xff000000 && row[14]==0xff14283c);
    if(argc>1) {
        NEResourceTable exe={0};assert(ne_load(&exe,argv[1])==0);CT_CursorBank bank={0};assert(CT_CursorsLoad(&bank,r,&exe)==4);
        for(int i=1;i<5;i++) {assert(bank.cursors[i].ready && bank.cursors[i].w==32 && bank.cursors[i].h==32);CT_CursorsDraw(&bank,&p,r,i);}
        assert(bank.cursors[CT_CURSOR_BUILD].x==7 && bank.cursors[CT_CURSOR_HAND].y==0);
        ne_free(&exe);
    }
    CT_DestroyTextures(r);SDL_DestroyRenderer(r);SDL_FreeSurface(screen);SDL_Quit();puts("PASS cursor masks: black/white/transparent/XOR invert, bounds/hotspots, correct fallback stem, real EXE tool cursors");
}
