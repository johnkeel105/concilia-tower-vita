/* CPU sprite operations must never require renderer-target readback.
 * Host: cc -Isrc $(sdl2-config --cflags) tests/test_sprite_cpu.c
 *       src/sprites.c src/ne_resource.c $(sdl2-config --libs) -o /tmp/sprite-test
 */
#include "sprites.h"
#include <assert.h>
#include <stdio.h>

static Uint32 pixel(SDL_Surface *s, int x, int y)
{ return ((Uint32 *)((Uint8 *)s->pixels + y * s->pitch))[x]; }

int main(int argc, char **argv)
{
    assert(SDL_Init(0) == 0);
    SDL_Surface *screen = SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_ARGB8888);
    assert(screen);
    SDL_Renderer *r = SDL_CreateSoftwareRenderer(screen);
    assert(r);
    SDL_Surface *a = SDL_CreateRGBSurfaceWithFormat(0, 4, 4, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Surface *b = SDL_CreateRGBSurfaceWithFormat(0, 4, 4, 32, SDL_PIXELFORMAT_ARGB8888);
    assert(a && b);
    SDL_FillRect(a, NULL, SDL_MapRGBA(a->format, 255, 0, 0, 255));
    SDL_FillRect(b, NULL, SDL_MapRGBA(b->format, 0, 0, 255, 255));
    ((Uint32 *)a->pixels)[0] = SDL_MapRGBA(a->format, 255, 255, 255, 255);
    SpriteAtlas atlas = {0};
    atlas.count = 2; atlas.sprites[0].id = 1; atlas.sprites[1].id = 2;
    assert(sprites_replace_surface(&atlas.sprites[0], r, a) == 0);
    assert(sprites_replace_surface(&atlas.sprites[1], r, b) == 0);
    SDL_FreeSurface(a); SDL_FreeSurface(b);
    assert(sprites_apply_white_key(&atlas, r, 1) == 0);
    assert(sprites_compose_h(&atlas, r, 1, 2, 3) == 0);
    Sprite *h = sprites_find(&atlas, 3);
    assert(h && h->w == 8 && h->h == 4 && h->pixels);
    assert(pixel(h->pixels, 0, 0) == 0); /* transparent-keyed white */
    assert(pixel(h->pixels, 1, 0) == 0xffff0000u);
    assert(pixel(h->pixels, 4, 0) == 0xff0000ffu);
    assert(sprites_compose_v(&atlas, r, 3, 2, 4) == 0);
    Sprite *v = sprites_find(&atlas, 4);
    assert(v && v->w == 8 && v->h == 8);
    assert(pixel(v->pixels, 0, 4) == 0xff0000ffu);
    assert(pixel(v->pixels, 7, 7) == 0); /* unused area cleared */
    sprites_free(&atlas);
    if (argc > 1) {
        NEResourceTable exe;
        assert(ne_load(&exe, argv[1]) == 0);
        assert(sprites_init(&atlas, &exe, r) == 0);
        size_t bytes = 0;
        for (int i = 0; i < atlas.count; i++) {
            assert(atlas.sprites[i].pixels);
            bytes += atlas.sprites[i].pixels->pitch * atlas.sprites[i].pixels->h;
        }
        printf("Real EXE: textures=%d CPU source bytes=%zu\n", atlas.count, bytes);
        assert(sprites_compose_h(&atlas, r, 0x86e8, 0x86e9, 0x00f0) == 0);
        assert(sprites_apply_white_key(&atlas, r, 0x00f0) == 0);
        sprites_free(&atlas);
        ne_free(&exe);
    }
    SDL_DestroyRenderer(r); SDL_FreeSurface(screen); SDL_Quit();
    puts("PASS: replacement, white key, horizontal/vertical composition, transparent padding, cleanup");
    return 0;
}
