/* Vita / vitaGL helpers for the ConciliaTower port. */
#include "vita_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <SDL_ttf.h>
#include <psp2/io/stat.h>
#include <psp2/power.h>

#define CT_DATA_DIR "ux0:data/concilia-tower"

extern int ct_kwaj_expand(const char *in_path, const char *out_path);

void vita_boost_clocks(void)
{
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
}

void vita_prepare_userdata(void)
{
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir(CT_DATA_DIR, 0777);
}

const char *vita_save_path(void)
{
    return CT_DATA_DIR "/concilliatower.sav";
}

void vita_try_expand_ex_(void)
{
    char dest[160];
    snprintf(dest, sizeof dest, "%s/SIMTOWER.EXE", CT_DATA_DIR);
    FILE *have = fopen(dest, "rb");
    if (have) {
        fclose(have);
        return;
    }

    static const char *cands[] = {
        CT_DATA_DIR "/SIMTOWER.EX_",
        CT_DATA_DIR "/simtower.ex_",
        "ux0:data/SIMTOWER.EX_",
        "ux0:data/simtower.ex_",
        NULL
    };
    for (int i = 0; cands[i]; i++) {
        FILE *t = fopen(cands[i], "rb");
        if (!t)
            continue;
        fclose(t);
        fprintf(stderr, "Expanding %s -> %s\n", cands[i], dest);
        ct_kwaj_expand(cands[i], dest);
        return;
    }
}

void vita_init_runtime(void)
{
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameControllerOpen(i);
            break;
        }
    }
}

int vita_remap_event(SDL_Event *ev)
{
    if (ev->type != SDL_CONTROLLERBUTTONDOWN &&
        ev->type != SDL_CONTROLLERBUTTONUP)
        return 0;

    int down = (ev->type == SDL_CONTROLLERBUTTONDOWN);
    SDL_Keycode k = SDLK_UNKNOWN;
    switch (ev->cbutton.button) {
    case SDL_CONTROLLER_BUTTON_A:            k = SDLK_RETURN;     break; /* Cross */
    case SDL_CONTROLLER_BUTTON_B:            k = SDLK_ESCAPE;     break; /* Circle */
    case SDL_CONTROLLER_BUTTON_START:        k = SDLK_SPACE;      break;
    case SDL_CONTROLLER_BUTTON_BACK:         k = SDLK_BACKQUOTE;  break; /* Select */
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:    k = SDLK_LEFT;       break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:   k = SDLK_RIGHT;      break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:      k = SDLK_UP;         break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:    k = SDLK_DOWN;       break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: k = SDLK_MINUS;      break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:k = SDLK_PLUS;       break;
    case SDL_CONTROLLER_BUTTON_Y:            k = SDLK_F5;         break; /* Triangle: save */
    default:
        return 1;
    }

    ev->type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev->key.keysym.sym = k;
    ev->key.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev->key.repeat = 0;
    ev->key.keysym.mod = KMOD_NONE;
    ev->key.keysym.scancode = SDL_GetScancodeFromKey(k);
    return 0;
}

void vita_pan_camera(float *cam_fx, float *cam_fy)
{
    static SDL_GameController *gc;
    if (!gc) {
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                gc = SDL_GameControllerOpen(i);
                break;
            }
        }
        if (!gc)
            return;
    }

    const int dead = 8000;
    int ax = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
    int ay = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
    int rx = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX);
    int ry = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY);
    if (abs(ax) > dead) *cam_fx += (ax / 32768.0f) * 14.0f;
    if (abs(ay) > dead) *cam_fy += (ay / 32768.0f) * 14.0f;
    if (abs(rx) > dead) *cam_fx += (rx / 32768.0f) * 26.0f;
    if (abs(ry) > dead) *cam_fy += (ry / 32768.0f) * 26.0f;
}

static TTF_Font *open_ui_font(int size)
{
    static const char *paths[] = {
        "app0:fonts/LiberationSans-Regular.ttf",
        "app0:/fonts/LiberationSans-Regular.ttf",
        "app0:fonts/LiberationSans-Bold.ttf",
        "app0:/fonts/LiberationSans-Bold.ttf",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        TTF_Font *f = TTF_OpenFont(paths[i], size);
        if (f)
            return f;
    }
    return NULL;
}

static void blit_text(SDL_Renderer *r, TTF_Font *font, const char *text,
                      int x, int y, SDL_Color color)
{
    if (!font || !text || !text[0])
        return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, color);
    if (!s)
        return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    if (t) {
        SDL_Rect d = { x, y, s->w, s->h };
        SDL_RenderCopy(r, t, NULL, &d);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

int vita_missing_exe_screen(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0)
        return 1;
    vita_init_runtime();
    TTF_Init();

    SDL_Window *w = SDL_CreateWindow("ConciliaTower",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        960, 544, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
    SDL_Renderer *r = NULL;
    if (w)
        r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    if (!r && w)
        r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
    if (!w || !r) {
        if (r) SDL_DestroyRenderer(r);
        if (w) SDL_DestroyWindow(w);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font *title = open_ui_font(26);
    TTF_Font *body = open_ui_font(18);
    const char *lines[] = {
        "SIMTOWER.EXE was not found.",
        "Copy a legally obtained copy to:",
        "",
        "ux0:data/concilia-tower/SIMTOWER.EXE",
        "",
        "Original CD/floppy media may ship SIMTOWER.EX_",
        "instead — drop that in the same folder and relaunch.",
        "This homebrew does not include Maxis assets.",
        "",
        "Press START or Circle to exit."
    };

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;
            else if (e.type == SDL_KEYDOWN &&
                     (e.key.keysym.sym == SDLK_ESCAPE ||
                      e.key.keysym.sym == SDLK_SPACE ||
                      e.key.keysym.sym == SDLK_RETURN))
                running = 0;
            else if (e.type == SDL_CONTROLLERBUTTONDOWN &&
                     (e.cbutton.button == SDL_CONTROLLER_BUTTON_START ||
                      e.cbutton.button == SDL_CONTROLLER_BUTTON_B ||
                      e.cbutton.button == SDL_CONTROLLER_BUTTON_A))
                running = 0;
        }
        SDL_SetRenderDrawColor(r, 14, 22, 48, 255);
        SDL_RenderClear(r);
        SDL_Color gold = { 235, 205, 92, 255 };
        SDL_Color cream = { 230, 228, 214, 255 };
        blit_text(r, title ? title : body, "ConciliaTower for PS Vita", 80, 70, gold);
        int y = 130;
        for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
            blit_text(r, body, lines[i], 80, y, cream);
            y += 28;
        }
        SDL_RenderPresent(r);
        SDL_Delay(16);
    }

    if (title) TTF_CloseFont(title);
    if (body) TTF_CloseFont(body);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    TTF_Quit();
    SDL_Quit();
    return 1;
}
