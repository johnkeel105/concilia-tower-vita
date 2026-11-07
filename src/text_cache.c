/* Immutable text raster + GPU cache. Callers retain existing SDL_FreeSurface /
 * CT_DestroyTexture ownership conventions; one cache reference stays alive. */
#include "text_cache.h"
#include <stdlib.h>
#include <string.h>
#define SLOTS 256
#define BYTE_BUDGET (4u*1024u*1024u)
typedef struct {
    TTF_Font *font;char *text;SDL_Color color;int utf8;
    SDL_Surface *surface;CT_Texture *texture;SDL_Renderer *renderer;
    unsigned age,hash;size_t bytes;
} Entry;
static Entry entries[SLOTS];
static unsigned serial;
static size_t used;
static CT_TextStats stats;
CT_TextStats CT_TextStatsReset(void) { CT_TextStats s=stats; memset(&stats,0,sizeof(stats));return s; }
static void discard(Entry *e)
{
    if(!e->surface) return;
    used-=e->bytes;CT_DestroyTexture(e->texture);SDL_FreeSurface(e->surface);SDL_free(e->text);memset(e,0,sizeof(*e));
}
void CT_TextCacheClear(void)
{ for(int i=0;i<SLOTS;i++) discard(&entries[i]); }
static Entry *oldest(void)
{
    Entry *old=NULL;
    for(int i=0;i<SLOTS;i++) if(entries[i].surface && (!old || entries[i].age<old->age)) old=&entries[i];
    return old;
}
static SDL_Surface *render(TTF_Font *font,const char *text,SDL_Color color,int utf8)
{
    if(!font||!text) return NULL;
    unsigned hash=2166136261u;for(const unsigned char *p=(const unsigned char *)text;*p;p++) hash=(hash^*p)*16777619u;
    for(int i=0;i<SLOTS;i++) {
        Entry *e=&entries[i];
        if(e->surface && e->hash==hash && e->font==font && e->utf8==utf8 &&
           !memcmp(&e->color,&color,sizeof(color)) && !strcmp(e->text,text)) {
            stats.hits++;e->age=++serial;e->surface->refcount++;return e->surface;
        }
    }
    stats.misses++;
    SDL_Surface *s=utf8?TTF_RenderUTF8_Blended(font,text,color):TTF_RenderText_Blended(font,text,color);
    if(!s) return NULL;
    size_t bytes=(size_t)s->pitch*s->h;
    if(bytes>BYTE_BUDGET) return s;
    while(used+bytes>BYTE_BUDGET) discard(oldest());
    Entry *e=NULL;
    for(int i=0;i<SLOTS;i++) if(!entries[i].surface) { e=&entries[i];break; }
    if(!e) e=oldest();
    discard(e);
    char *copy=SDL_strdup(text);if(!copy) return s;
    e->font=font;e->text=copy;e->color=color;e->utf8=utf8;e->surface=s;
    e->age=++serial;e->hash=hash;e->bytes=bytes;used+=bytes;s->refcount++;
    return s;
}
SDL_Surface *CT_RenderUTF8_Blended(TTF_Font *f,const char *s,SDL_Color c) { return render(f,s,c,1); }
SDL_Surface *CT_RenderText_Blended(TTF_Font *f,const char *s,SDL_Color c) { return render(f,s,c,0); }
CT_Texture *CT_TextTexture(SDL_Renderer *r,SDL_Surface *s)
{
    for(int i=0;i<SLOTS;i++) {
        Entry *e=&entries[i];
        if(e->surface==s && s) {
            if(e->texture && e->renderer!=r) { CT_DestroyTexture(e->texture);e->texture=NULL; }
            if(!e->texture) { e->texture=CT_CreateTextureFromSurface(r,s);e->renderer=r; }
            return CT_RetainTexture(e->texture);
        }
    }
    return CT_CreateTextureFromSurface(r,s);
}
