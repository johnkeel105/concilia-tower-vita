#ifndef CT_UI_FOCUS_H
#define CT_UI_FOCUS_H
#include <SDL.h>
/* Directions: left=0 right=1 up=2 down=3. No allocation or rendering uploads. */
int CT_UIFocusNext(const SDL_Rect *,int count,int current,int direction);
#endif
