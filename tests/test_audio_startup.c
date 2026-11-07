/* Decode/mix every sound offline; no host or Vita device needed.
 * cc -Isrc $(sdl2-config --cflags) tests/test_audio_startup.c src/audio.c
 *    src/ne_resource.c $(sdl2-config --libs) -o /tmp/audio-startup-test
 */
#include "audio.h"
#include "ne_resource.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    assert(argc==2 && SDL_Init(0)==0);
    NEResourceTable exe;
    assert(ne_load(&exe,argv[1])==0);
    assert(audio_init_capture()==0);
    int count=audio_load_from_ne(&exe);
    assert(count>0);
    NEResourceList *sounds=ne_find_type(&exe,NE_RT_SOUND);
    assert(sounds && count==sounds->count);
    for(int i=0;i<sounds->count;i++) {
        assert(audio_has_clip(sounds->items[i].id));
        audio_play(sounds->items[i].id,0.5f);
        audio_advance(AUDIO_DEV_FREQ/20);
    }
    audio_shutdown(); ne_free(&exe); SDL_Quit();
    printf("PASS: decoded and mixed all %d sounds without device\n",count);
}
